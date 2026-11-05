#include "inkay/updater/install.hh"
#include "inkay/updater/fsutil.hh"
#include "inkay/updater/rplmeta.hh"
#include "eg2/libs/nlohmannjson.hh"
#include "eg2/libs/miniz.hh"
#include <cstring>

namespace Updater {
    bool ZlibUncompress(uint8_t* dst, size_t* dstLen, const uint8_t* src, size_t srcLen) {
        mz_ulong len = *dstLen;
        if (mz_uncompress(dst, &len, src, (mz_ulong)srcLen) != MZ_OK) return false;
        *dstLen = len;
        return true;
    }

    struct ZipFile {
        std::string name;
        std::vector<uint8_t> data;
    };

    // Plugins and modules are ELF files
    static bool LooksLikeElf(const std::vector<uint8_t>& data) {
        return data.size() > 1024 && data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F';
    }

    // Finds the one .wms or .wps in the zip. When there are several, the one
    // with "inkay" in its name is used; still ambiguous means failure.
    static bool PickFile(mz_zip_archive& zip, const std::string& ext, ZipFile& out, std::string& error) {
        int count = (int)mz_zip_reader_get_num_files(&zip);
        std::vector<int> all, inkay;

        for (int i = 0; i < count; i++) {
            mz_zip_archive_file_stat st;
            if (!mz_zip_reader_file_stat(&zip, i, &st)) continue;
            if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;

            std::string name = FS::Lower(st.m_filename);
            if (!FS::EndsWith(name, ext)) continue;
            all.push_back(i);
            size_t slash = name.find_last_of('/');
            if (name.substr(slash == std::string::npos ? 0 : slash + 1).find("inkay") != std::string::npos) inkay.push_back(i);
        }

        int index = -1;
        if (all.size() == 1) index = all[0];
        else if (inkay.size() == 1) index = inkay[0];

        if (index < 0) {
            error = all.empty() ? "The download has no " + ext + " file" : "The download has more than one " + ext + " file";
            return false;
        }

        mz_zip_archive_file_stat st;
        mz_zip_reader_file_stat(&zip, index, &st);
        out.name = st.m_filename;

        size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&zip, index, &size, 0);
        if (!data) {
            error = "Couldn't unpack " + out.name;
            return false;
        }
        out.data.assign((uint8_t*)data, (uint8_t*)data + size);
        mz_free(data);

        if (!LooksLikeElf(out.data)) {
            error = out.name + " is not a valid " + ext + " file";
            return false;
        }
        return true;
    }

    // Moves a new file into place, keeping the old one as .bak until done.
    struct Swap {
        std::string target, fresh, backup;
        bool hadOld = false;
        bool movedOld = false;
        bool placed = false;
    };

    static void Rollback(std::vector<Swap>& swaps) {
        for (auto it = swaps.rbegin(); it != swaps.rend(); ++it) {
            if (it->placed) FS::Remove(it->target);
            if (it->movedOld) FS::Rename(it->backup, it->target);
            FS::Remove(it->fresh);
        }
    }

    InstallResult Install(const std::string& envPath, Net net, const std::string& tag, const std::vector<uint8_t>& zipData) {
        InstallResult result;

        if (zipData.size() < 22) {
            result.message = "The download is empty";
            return result;
        }

        // 1. Unpack and check everything before touching the SD card
        mz_zip_archive zip;
        memset(&zip, 0, sizeof(zip));
        if (!mz_zip_reader_init_mem(&zip, zipData.data(), zipData.size(), 0)) {
            result.message = "The download is not a valid zip";
            return result;
        }

        ZipFile wms, wps;
        std::string error;
        bool ok = PickFile(zip, ".wms", wms, error) && PickFile(zip, ".wps", wps, error);
        mz_zip_reader_end(&zip);
        if (!ok) {
            result.message = error;
            return result;
        }

        // 2. Write the new files next to the old ones and read them back
        std::vector<Swap> swaps;
        for (auto& [rel, file] : {std::pair<const char*, ZipFile*>{WMS_NAME, &wms}, {WPS_NAME, &wps}}) {
            Swap s;
            s.target = envPath + "/" + rel;
            s.fresh = s.target + ".new"; // doesn't end in .wps/.wms, so Aroma never loads it
            s.backup = s.target + ".bak";
            swaps.push_back(s);

            std::string expected = FS::Sha1Hex(file->data.data(), file->data.size());
            if (!FS::WriteAll(s.fresh, file->data.data(), file->data.size()) || FS::Sha1File(s.fresh) != expected) {
                Rollback(swaps);
                result.message = "Couldn't write to the SD card";
                return result;
            }
        }

        // 3. Swap: old -> .bak, new -> name. Any failure puts the old files back.
        for (Swap& s : swaps) {
            s.hadOld = FS::Exists(s.target);
            if (s.hadOld) {
                FS::Remove(s.backup);
                if (!FS::Rename(s.target, s.backup)) {
                    Rollback(swaps);
                    result.message = "Couldn't replace the old Inkay, nothing was changed";
                    return result;
                }
                s.movedOld = true;
            }
            if (!FS::Rename(s.fresh, s.target)) {
                Rollback(swaps);
                result.message = "Couldn't replace the old Inkay, nothing was changed";
                return result;
            }
            s.placed = true;
        }

        for (Swap& s : swaps) if (s.movedOld) FS::Remove(s.backup);

        // 4. Other Inkay files would load next to the new one: switch them off
        for (const char* folder : {"modules", "plugins"}) {
            for (const std::string& file : FS::ListFiles(envPath + "/" + folder)) {
                std::string lower = FS::Lower(file);
                std::string rel = std::string(folder) + "/" + file;
                bool inkay = lower.find("inkay") != std::string::npos && (FS::EndsWith(lower, ".wps") || FS::EndsWith(lower, ".wms"));
                if (!inkay || rel == WMS_NAME || rel == WPS_NAME) continue;

                std::string from = envPath + "/" + rel;
                if (FS::Rename(from, from + ".disabled")) result.disabled.push_back(rel);
            }
        }

        // 5. Remember what we installed, so the next start knows the version
        nlohmann::json marker = {
            {"network", Info(net).id},
            {"tag", tag},
            {"wms", FS::Sha1Hex(wms.data.data(), wms.data.size())},
            {"wps", FS::Sha1Hex(wps.data.data(), wps.data.size())},
            {"wms_source", wms.name},
            {"wps_source", wps.name}};
        std::string text = marker.dump(4, ' ', false, nlohmann::json::error_handler_t::replace);
        FS::WriteAll(envPath + "/" + MARKER_NAME, text.data(), text.size());

        result.ok = true;
        result.message = std::string("Installed ") + Info(net).name + " Inkay " + tag;
        return result;
    }
}
