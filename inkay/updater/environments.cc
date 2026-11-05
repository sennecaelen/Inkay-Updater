#include "inkay/updater/environments.hh"
#include "inkay/updater/fsutil.hh"
#include "inkay/updater/rplmeta.hh"
#include "eg2/libs/nlohmannjson.hh"
#include <algorithm>
#include <cstdint>

namespace Updater {
    namespace Environments {
        // The plugin's own metadata says which network it is for, e.g.
        // "name=Protarium" / "description=Protarium Network Patcher".
        static bool IsProtarium(const PluginMeta& meta) {
            return FS::Lower(meta.name + " " + meta.description + " " + meta.version).find("protarium") != std::string::npos;
        }

        static bool IsInkayFile(const std::string& name) {
            std::string lower = FS::Lower(name);
            return lower.find("inkay") != std::string::npos && (FS::EndsWith(lower, ".wps") || FS::EndsWith(lower, ".wms"));
        }

        Environment ScanOne(const std::string& root, const std::string& name) {
            Environment env;
            env.name = name;
            env.path = root + "/" + name;

            const std::string wms = env.path + "/" + WMS_NAME;
            const std::string wps = env.path + "/" + WPS_NAME;
            bool hasWms = FS::Exists(wms);
            bool hasWps = FS::Exists(wps);

            if (hasWms || hasWps) {
                PluginMeta meta = hasWps ? ReadPluginMetaFile(wps) : PluginMeta{};
                if (!meta.ok && hasWms) meta = ReadPluginMetaFile(wms);
                env.installed = IsProtarium(meta) ? Net::Protarium : Net::Pretendo;
                env.installedVersion = meta.version;
            }

            // Other Inkay builds under a different file name would load next to ours
            for (const char* folder : {"modules", "plugins"}) {
                for (const std::string& file : FS::ListFiles(env.path + "/" + folder)) {
                    std::string rel = std::string(folder) + "/" + file;
                    if (IsInkayFile(file) && rel != WMS_NAME && rel != WPS_NAME) env.otherInkayFiles.push_back(rel);
                }
            }

            // No version in the plugin: fall back to what this app wrote when it
            // installed the files, if they haven't been changed since
            std::vector<uint8_t> marker;
            if (env.installedVersion.empty() && env.installed != Net::None && hasWms && hasWps && FS::ReadAll(env.path + "/" + MARKER_NAME, marker)) {
                auto j = nlohmann::json::parse(marker.begin(), marker.end(), nullptr, false);
                if (!j.is_discarded() && j.is_object() &&
                    j.contains("network") && j["network"].is_string() &&
                    j.contains("tag") && j["tag"].is_string() &&
                    j.contains("wms") && j["wms"].is_string() &&
                    j.contains("wps") && j["wps"].is_string()) {
                    bool same = NetFromId(j["network"].get<std::string>()) == env.installed &&
                                FS::Sha1File(wms) == j["wms"].get<std::string>() &&
                                FS::Sha1File(wps) == j["wps"].get<std::string>();
                    if (same) env.installedVersion = j["tag"].get<std::string>();
                }
            }

            // What to install: keep the current network, otherwise go by the name
            if (env.installed != Net::None) env.target = env.installed;
            else env.target = FS::Lower(name).find("protarium") != std::string::npos ? Net::Protarium : Net::Pretendo;

            return env;
        }

        std::vector<Environment> Scan(const std::string& root) {
            std::vector<std::string> names = FS::ListDirs(root);
            std::sort(names.begin(), names.end(), [](const std::string& a, const std::string& b) { return FS::Lower(a) < FS::Lower(b); });

            std::vector<Environment> out;
            for (const std::string& name : names) out.push_back(ScanOne(root, name));
            return out;
        }

        Status GetStatus(const Environment& env, const std::string& latestTag) {
            if (env.installed == Net::None) return Status::NotInstalled;
            if (env.target != env.installed) return Status::Switch;
            if (latestTag.empty()) return Status::NoRelease;
            if (env.installedVersion.empty()) return Status::UnknownVersion;
            return env.installedVersion == latestTag ? Status::UpToDate : Status::UpdateAvailable;
        }

        const char* StatusText(Status status) {
            switch (status) {
                case Status::NotInstalled: return "Not installed";
                case Status::UpToDate: return "Up to date";
                case Status::UpdateAvailable: return "Update available";
                case Status::UnknownVersion: return "Version unknown";
                case Status::Switch: return "Switch network";
                case Status::NoRelease: return "Can't check for updates";
            }
            return "";
        }
    }
}
