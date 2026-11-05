#pragma once

#include "inkay/updater/environments.hh"
#include "inkay/updater/install.hh"
#include <string>
#include <vector>

namespace Updater {
    namespace App {
        enum class Screen {
            Checking,    // looking up the latest releases and scanning the SD card
            List,        // environments with their status
            Downloading,
            Done,
            Error
        };

        Screen GetScreen(void);
        const std::vector<Environment>& GetEnvironments(void);
        int GetSelected(void);
        const InstallResult& GetResult(void);
        const std::string& GetError(void);
        const std::string& GetBusyText(void);

        // Latest version for a network, "" when it couldn't be checked
        std::string LatestTag(Net net);
        Status GetStatus(const Environment& env);

        void Start(void);               // check releases + scan, in the background
        void Update(void);              // call every frame
        void MoveSelection(int delta);
        void ToggleTarget(void);        // Pretendo <-> Protarium for the selected environment
        void InstallSelected(void);
        void BackToList(void);
    }
}
