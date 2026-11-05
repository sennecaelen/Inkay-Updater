#include "inkay/updater/app.hh"
#include "inkay/updater/releases.hh"
#include "eg2/network/curl.hh"
#include "eg2/sys/report.hh"
#include <atomic>
#include <cstdint>
#include <thread>

namespace Updater {
    namespace App {
        static std::atomic<Screen> gScreen{Screen::Checking};
        static std::atomic<bool> gCheckDone{false};
        static std::vector<Environment> gEnvs;
        static std::vector<Environment> gScanned; // written by the background thread
        static int gSelected = 0;

        static InstallResult gResult;
        static std::string gError;
        static std::string gBusy = "Checking for updates...";

        static std::vector<uint8_t> gZip;
        static bool gDownloading = false;
        static int gInstallIndex = -1;
        static Net gInstallNet = Net::None;
        static std::string gInstallTag;

        Screen GetScreen(void) { return gScreen.load(); }
        const std::vector<Environment>& GetEnvironments(void) { return gEnvs; }
        int GetSelected(void) { return gSelected; }
        const InstallResult& GetResult(void) { return gResult; }
        const std::string& GetError(void) { return gError; }
        const std::string& GetBusyText(void) { return gBusy; }

        std::string LatestTag(Net net) {
            const Release& r = Releases::Get(net);
            return r.ok ? r.tag : "";
        }

        Status GetStatus(const Environment& env) {
            return Environments::GetStatus(env, LatestTag(env.target));
        }

        void Start(void) {
            gScreen = Screen::Checking;
            gBusy = "Checking for updates...";
            gCheckDone = false;

            std::thread([]() {
                Releases::FetchAll();
                gScanned = Environments::Scan(ENVIRONMENTS_ROOT);
                gCheckDone = true;
            }).detach();
        }

        void Update(void) {
            if (gScreen == Screen::Checking && gCheckDone.exchange(false)) {
                gEnvs = gScanned;
                if (gSelected >= (int)gEnvs.size()) gSelected = 0;

                if (gEnvs.empty()) {
                    gError = "No Aroma environment found on the SD card\n(sd:/wiiu/environments)";
                    gScreen = Screen::Error;
                } else {
                    gScreen = Screen::List;
                }
            }

            if (gScreen == Screen::Downloading && gDownloading && Network::IsDownloadFinished()) {
                gDownloading = false;

                if (!Network::WasDownloadSuccessful()) {
                    gError = "The download failed. Check your internet connection\nand try again. Nothing was changed.";
                    gScreen = Screen::Error;
                    return;
                }

                Environment& env = gEnvs[gInstallIndex];
                gResult = Install(env.path, gInstallNet, gInstallTag, gZip);
                gZip.clear();
                gZip.shrink_to_fit();

                SYS::Report::Log("Updater: install into %s: %s\n", env.name.c_str(), gResult.message.c_str());

                Net keepTarget = env.target;
                env = Environments::ScanOne(ENVIRONMENTS_ROOT, env.name);
                if (!gResult.ok) env.target = keepTarget;

                if (gResult.ok) {
                    gScreen = Screen::Done;
                } else {
                    gError = gResult.message;
                    gScreen = Screen::Error;
                }
            }
        }

        void MoveSelection(int delta) {
            if (gEnvs.empty()) return;
            int n = (int)gEnvs.size();
            gSelected = ((gSelected + delta) % n + n) % n;
        }

        void ToggleTarget(void) {
            if (gEnvs.empty()) return;
            Environment& env = gEnvs[gSelected];
            env.target = env.target == Net::Protarium ? Net::Pretendo : Net::Protarium;
        }

        void InstallSelected(void) {
            if (gScreen != Screen::List || gEnvs.empty()) return;

            Environment& env = gEnvs[gSelected];
            const Release& release = Releases::Get(env.target);
            if (!release.ok) {
                gError = std::string("Couldn't check the latest ") + Info(env.target).name + " Inkay:\n" + release.error;
                gScreen = Screen::Error;
                return;
            }

            gInstallIndex = gSelected;
            gInstallNet = env.target;
            gInstallTag = release.tag;
            gBusy = std::string("Downloading ") + Info(env.target).name + " Inkay " + release.tag + "\nfor \"" + env.name + "\"...";
            gZip.clear();
            gDownloading = true;
            gScreen = Screen::Downloading;
            Network::DownloadAsync(release.url, &gZip);
        }

        void BackToList(void) {
            if (gScreen != Screen::Done && gScreen != Screen::Error) return;
            if (gEnvs.empty()) Start(); // nothing to go back to: look again
            else gScreen = Screen::List;
        }
    }
}
