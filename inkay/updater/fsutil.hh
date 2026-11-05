#pragma once

// Small file helpers on top of stdio/dirent, so they work the same on the
// Wii U (through the SD card devoptab) and on a computer (for tests).

#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>

namespace Updater {
    namespace FS {
        bool ReadAll(const std::string& path, std::vector<uint8_t>& out);
        bool WriteAll(const std::string& path, const void* data, size_t size);
        bool Exists(const std::string& path);
        bool Remove(const std::string& path);
        bool Rename(const std::string& from, const std::string& to);
        void MakeDirs(const std::string& dir);

        std::vector<std::string> ListDirs(const std::string& path);
        std::vector<std::string> ListFiles(const std::string& path);

        std::string Lower(std::string s);
        bool EndsWith(const std::string& s, const std::string& suffix);
        bool ContainsCI(const uint8_t* data, size_t size, const char* needle);

        std::string Sha1Hex(const void* data, size_t size);
        std::string Sha1File(const std::string& path); // "" when the file can't be read
    }
}
