#pragma once

#include <string>

namespace Updater {
    enum class Net {
        None,      // no Inkay installed (Nintendo Network)
        Pretendo,
        Protarium
    };

    struct NetInfo {
        const char* name;          // shown on screen
        const char* id;            // stored in the marker file
        const char* releaseApi;    // GitHub "latest release" endpoint
        const char* preferredAsset;
    };

    inline const NetInfo& Info(Net net) {
        static const NetInfo none      = {"Nintendo Network", "none", "", ""};
        static const NetInfo pretendo  = {"Pretendo", "pretendo",
                                          "https://api.github.com/repos/PretendoNetwork/Inkay/releases/latest",
                                          "Inkay-pretendo.zip"};
        static const NetInfo protarium = {"Protarium", "protarium",
                                          "https://api.github.com/repos/Protarium-Network/Inkay-GitHub-Release/releases/latest",
                                          "wiiu.zip"};
        switch (net) {
            case Net::Pretendo: return pretendo;
            case Net::Protarium: return protarium;
            default: return none;
        }
    }

    inline Net NetFromId(const std::string& id) {
        if (id == "pretendo") return Net::Pretendo;
        if (id == "protarium") return Net::Protarium;
        return Net::None;
    }

    // Installed files. Both networks use the same names, like their own
    // installers do, so switching replaces the other network's Inkay.
    inline const char* WMS_NAME = "modules/Inkay-pretendo.wms";
    inline const char* WPS_NAME = "plugins/Inkay-pretendo.wps";
    inline const char* MARKER_NAME = "plugins/config/inkay_updater.json";
    inline const char* ENVIRONMENTS_ROOT = "fs:/vol/external01/wiiu/environments";
}
