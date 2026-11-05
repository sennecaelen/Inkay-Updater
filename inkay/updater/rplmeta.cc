#include "inkay/updater/rplmeta.hh"
#include "inkay/updater/fsutil.hh"
#include <cstring>


namespace Updater {
    static uint32_t BE32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
    static uint16_t BE16(const uint8_t* p) { return (uint16_t)((p[0] << 8) | p[1]); }

    static constexpr uint32_t SHT_NOBITS = 8;
    static constexpr uint32_t SHF_RPL_ZLIB = 0x08000000;
    static constexpr uint32_t MAX_SECTION = 16 * 1024 * 1024;

    bool UnpackRpl(const std::vector<uint8_t>& file, std::vector<uint8_t>& out) {
        out.clear();
        const size_t size = file.size();
        if (size < 0x34 || memcmp(file.data(), "\x7F" "ELF", 4) != 0) return false;
        if (file[4] != 1 || file[5] != 2) return false; // 32-bit, big endian

        const uint8_t* d = file.data();
        uint32_t shoff = BE32(d + 0x20);
        uint16_t shentsize = BE16(d + 0x2E);
        uint16_t shnum = BE16(d + 0x30);
        if (shentsize < 40 || shoff > size || (uint64_t)shoff + (uint64_t)shnum * shentsize > size) return false;

        for (uint16_t i = 0; i < shnum; i++) {
            const uint8_t* h = d + shoff + (size_t)i * shentsize;
            uint32_t type = BE32(h + 4);
            uint32_t flags = BE32(h + 8);
            uint32_t offset = BE32(h + 16);
            uint32_t length = BE32(h + 20);
            if (type == SHT_NOBITS || length == 0) continue;
            if ((uint64_t)offset + length > size) continue;

            const uint8_t* src = d + offset;
            if (flags & SHF_RPL_ZLIB) {
                if (length < 4) continue;
                uint32_t inflated = BE32(src);
                if (inflated == 0 || inflated > MAX_SECTION) continue;

                size_t start = out.size();
                out.resize(start + inflated);
                size_t outLen = inflated;
                if (!ZlibUncompress(out.data() + start, &outLen, src + 4, length - 4)) {
                    out.resize(start);
                    continue;
                }
                out.resize(start + outLen);
            } else {
                out.insert(out.end(), src, src + length);
            }
            out.push_back('\n');
        }
        return !out.empty();
    }

    // Finds "key=" at the start of a string and returns the text up to the
    // terminating zero byte.
    static std::string FindValue(const std::vector<uint8_t>& data, const char* key) {
        const size_t len = strlen(key);
        for (size_t i = 0; i + len < data.size(); i++) {
            if (memcmp(data.data() + i, key, len) != 0) continue;
            if (i > 0 && data[i - 1] != 0 && data[i - 1] != '\n') continue; // must start a string

            size_t end = i + len;
            while (end < data.size() && data[end] >= 0x20 && data[end] < 0x7F && end - i < 256) end++;
            if (end < data.size() && data[end] == 0) return std::string((const char*)data.data() + i + len, end - i - len);
        }
        return "";
    }

    PluginMeta ReadPluginMeta(const std::vector<uint8_t>& file) {
        PluginMeta meta;
        std::vector<uint8_t> unpacked;
        if (!UnpackRpl(file, unpacked)) return meta;

        meta.name = FindValue(unpacked, "name=");
        meta.version = FindValue(unpacked, "version=");
        meta.description = FindValue(unpacked, "description=");
        meta.author = FindValue(unpacked, "author=");
        meta.ok = !meta.name.empty() || !meta.description.empty();
        return meta;
    }

    PluginMeta ReadPluginMetaFile(const std::string& path) {
        std::vector<uint8_t> file;
        if (!FS::ReadAll(path, file)) return {};
        return ReadPluginMeta(file);
    }
}
