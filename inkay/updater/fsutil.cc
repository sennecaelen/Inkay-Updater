#include "inkay/updater/fsutil.hh"
#include "eg2/libs/tinysha1.hh"
#include <cstdio>
#include <cstring>
#include <cctype>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace Updater {
    namespace FS {
        bool ReadAll(const std::string& path, std::vector<uint8_t>& out) {
            out.clear();
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) return false;

            uint8_t buffer[16 * 1024];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.insert(out.end(), buffer, buffer + n);

            bool ok = !ferror(f);
            fclose(f);
            return ok;
        }

        bool WriteAll(const std::string& path, const void* data, size_t size) {
            size_t slash = path.find_last_of('/');
            if (slash != std::string::npos) MakeDirs(path.substr(0, slash));

            FILE* f = fopen(path.c_str(), "wb");
            if (!f) return false;

            size_t written = size ? fwrite(data, 1, size, f) : 0;
            bool ok = (written == size) && (fflush(f) == 0);
            if (fclose(f) != 0) ok = false;
            return ok;
        }

        bool Exists(const std::string& path) {
            struct stat st;
            return stat(path.c_str(), &st) == 0;
        }

        bool Remove(const std::string& path) {
            return remove(path.c_str()) == 0;
        }

        bool Rename(const std::string& from, const std::string& to) {
            return rename(from.c_str(), to.c_str()) == 0;
        }

        void MakeDirs(const std::string& dir) {
            std::string current;
            for (size_t i = 0; i < dir.size(); i++) {
                current += dir[i];
                if (dir[i] == '/' || i + 1 == dir.size()) mkdir(current.c_str(), 0777);
            }
        }

        static std::vector<std::string> List(const std::string& path, bool dirs) {
            std::vector<std::string> out;
            DIR* d = opendir(path.c_str());
            if (!d) return out;

            struct dirent* e;
            while ((e = readdir(d)) != nullptr) {
                std::string name = e->d_name;
                if (name == "." || name == "..") continue;

                struct stat st;
                bool isDir = stat((path + "/" + name).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
                if (isDir == dirs) out.push_back(name);
            }
            closedir(d);
            return out;
        }

        std::vector<std::string> ListDirs(const std::string& path) { return List(path, true); }
        std::vector<std::string> ListFiles(const std::string& path) { return List(path, false); }

        std::string Lower(std::string s) {
            for (char& c : s) c = (char)tolower((unsigned char)c);
            return s;
        }

        bool EndsWith(const std::string& s, const std::string& suffix) {
            return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
        }

        bool ContainsCI(const uint8_t* data, size_t size, const char* needle) {
            size_t len = strlen(needle);
            if (!len || size < len) return false;

            for (size_t i = 0; i + len <= size; i++) {
                size_t j = 0;
                while (j < len && tolower(data[i + j]) == tolower((unsigned char)needle[j])) j++;
                if (j == len) return true;
            }
            return false;
        }

        static std::string Hex(const sha1::SHA1::digest8_t digest) {
            char out[41];
            for (int i = 0; i < 20; i++) snprintf(out + i * 2, 3, "%02x", digest[i]);
            out[40] = '\0';
            return out;
        }

        std::string Sha1Hex(const void* data, size_t size) {
            sha1::SHA1 s;
            s.processBytes(data, size);
            sha1::SHA1::digest8_t digest;
            s.getDigestBytes(digest);
            return Hex(digest);
        }

        std::string Sha1File(const std::string& path) {
            FILE* f = fopen(path.c_str(), "rb");
            if (!f) return "";

            sha1::SHA1 s;
            uint8_t buffer[16 * 1024];
            size_t n;
            while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) s.processBytes(buffer, n);
            bool ok = !ferror(f);
            fclose(f);
            if (!ok) return "";

            sha1::SHA1::digest8_t digest;
            s.getDigestBytes(digest);
            return Hex(digest);
        }
    }
}
