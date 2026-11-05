#pragma once

#include "inkay/updater/networks.hh"
#include <string>
#include <vector>

namespace Updater {
    struct Environment {
        std::string name;          // folder name, e.g. "Aroma Protarium"
        std::string path;          // full path to the folder

        Net installed = Net::None; // which Inkay is in it now
        std::string installedVersion; // e.g. "v3.0.0", read from the plugin; empty when unknown
        Net target = Net::None;    // network to install, changed with left/right

        std::vector<std::string> otherInkayFiles; // extra Inkay files, would load twice
    };

    enum class Status {
        NotInstalled,
        UpToDate,
        UpdateAvailable,
        UnknownVersion, // installed, but its version can't be read
        Switch,         // the other network is selected
        NoRelease       // latest version couldn't be checked
    };

    namespace Environments {
        // Reads every folder in the environments root.
        std::vector<Environment> Scan(const std::string& root);
        Environment ScanOne(const std::string& root, const std::string& name);

        Status GetStatus(const Environment& env, const std::string& latestTag);
        const char* StatusText(Status status);
    }
}
