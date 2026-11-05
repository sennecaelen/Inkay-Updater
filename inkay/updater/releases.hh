#pragma once

#include "inkay/updater/networks.hh"
#include <string>

namespace Updater {
    struct Release {
        bool ok = false;
        std::string tag;   // e.g. "v2.7.0"
        std::string url;   // zip download URL
        std::string error; // why it failed, shown on screen
    };

    // Reads a GitHub "latest release" answer. Picks the preferred asset, or
    // the only .zip asset when the preferred name isn't there.
    Release ParseRelease(const std::string& json, const std::string& preferredAsset);

    namespace Releases {
        // Fetches the latest release of both networks (blocking, call from a
        // thread). Results are read with Get().
        void FetchAll(void);
        const Release& Get(Net net);
    }
}
