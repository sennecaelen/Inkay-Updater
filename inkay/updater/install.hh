#pragma once

#include "inkay/updater/networks.hh"
#include <cstdint>
#include <string>
#include <vector>

namespace Updater {
    struct InstallResult {
        bool ok = false;
        std::string message;               // shown on screen
        std::vector<std::string> disabled; // other Inkay files that were renamed
    };

    // Installs the Inkay files from a downloaded release zip into an
    // environment folder. Nothing is changed unless the zip has a valid
    // plugin and module; the old files are put back if replacing fails.
    InstallResult Install(const std::string& envPath, Net net, const std::string& tag,
                          const std::vector<uint8_t>& zip);
}
