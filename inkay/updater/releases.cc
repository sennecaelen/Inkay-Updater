#include "inkay/updater/releases.hh"
#include "inkay/updater/fsutil.hh"
#include "eg2/libs/nlohmannjson.hh"
#include <vector>

#ifndef UPDATER_HOST_TEST
#include "eg2/network/curl.hh"
#include "eg2/sys/report.hh"
#endif

namespace Updater {
    Release ParseRelease(const std::string& text, const std::string& preferredAsset) {
        Release r;

        auto j = nlohmann::json::parse(text, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            r.error = "GitHub sent an invalid answer";
            return r;
        }

        if (j.contains("message") && j["message"].is_string() && !j.contains("assets")) {
            r.error = "GitHub: " + j["message"].get<std::string>();
            return r;
        }

        if (j.contains("tag_name") && j["tag_name"].is_string()) r.tag = j["tag_name"].get<std::string>();

        std::vector<std::pair<std::string, std::string>> zips; // name, url
        if (j.contains("assets") && j["assets"].is_array()) {
            for (const auto& asset : j["assets"]) {
                if (!asset.is_object() || !asset.contains("name") || !asset.contains("browser_download_url")) continue;
                if (!asset["name"].is_string() || !asset["browser_download_url"].is_string()) continue;

                std::string name = asset["name"].get<std::string>();
                std::string url = asset["browser_download_url"].get<std::string>();
                if (FS::Lower(name) == FS::Lower(preferredAsset)) {
                    r.url = url;
                    break;
                }
                if (FS::EndsWith(FS::Lower(name), ".zip")) zips.push_back({name, url});
            }
        }

        if (r.url.empty() && zips.size() == 1) r.url = zips[0].second;

        if (r.tag.empty()) r.error = "Release has no version";
        else if (r.url.empty()) r.error = "Release has no " + preferredAsset;
        else r.ok = true;

        return r;
    }

    namespace Releases {
        static Release gPretendo;
        static Release gProtarium;
        static Release gNone;

        const Release& Get(Net net) {
            if (net == Net::Pretendo) return gPretendo;
            if (net == Net::Protarium) return gProtarium;
            return gNone;
        }

#ifndef UPDATER_HOST_TEST
        static Release Fetch(Net net) {
            const NetInfo& info = Info(net);
            Release r;

            if (!Network::SetConnection(info.releaseApi)) {
                r.error = "No connection: " + Network::GetLastError();
                SYS::Report::Log("Updater: %s lookup failed: %s\n", info.name, r.error.c_str());
                return r;
            }

            long code = Network::GetNumericCode();
            r = ParseRelease(Network::GetLastResponse(), info.preferredAsset);
            if (!r.ok && code != 200) r.error = "GitHub answered " + std::to_string(code);

            SYS::Report::Log("Updater: %s latest %s (%s) %s\n", info.name, r.tag.c_str(), r.url.c_str(), r.error.c_str());
            return r;
        }

        void FetchAll(void) {
            gPretendo = Fetch(Net::Pretendo);
            gProtarium = Fetch(Net::Protarium);
        }
#endif
    }
}
