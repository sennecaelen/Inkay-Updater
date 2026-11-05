#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Updater {
    // The WUPS metadata inside a plugin (.wps) or module (.wms), such as
    // "name=Protarium" and "version=v3.1.0-protarium-online-r17".
    struct PluginMeta {
        bool ok = false;
        std::string name;
        std::string version;
        std::string description;
        std::string author;
    };

    // Plugins and modules are RPL files with zlib-compressed sections, so the
    // text can't be searched directly. This unpacks the sections and reads the
    // "key=value" entries.
    PluginMeta ReadPluginMeta(const std::vector<uint8_t>& file);
    PluginMeta ReadPluginMetaFile(const std::string& path);

    // Unpacked contents of all sections, for searching text.
    bool UnpackRpl(const std::vector<uint8_t>& file, std::vector<uint8_t>& out);

    // zlib inflate; implemented in install.cc, the one file that includes miniz
    // (its header can only be included once).
    bool ZlibUncompress(uint8_t* dst, size_t* dstLen, const uint8_t* src, size_t srcLen);
}
