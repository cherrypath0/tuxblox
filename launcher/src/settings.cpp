// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "settings.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string settingsFilePath(const std::string& installDir) {
    return installDir + "/settings.json";
}

bool isKnownChannel(const std::string& channel) {
    return channel == "stable" || channel == "canary" || channel == "dev";
}

// TuxBlox used to keep two env var lists, one for the Proton child and one
// exported from the launcher. They are a single list now, so a settings.json
// written before the merge has its two values joined rather than one of them
// being silently dropped.
std::string mergeLegacyEnvVars(const nlohmann::json& j) {
    std::string proton = j.value("proton_env_vars", std::string());
    std::string global = j.value("global_env_vars", std::string());
    if (proton.empty()) return global;
    if (global.empty()) return proton;
    return proton + " " + global;
}

// Both directions are lenient about shape: a hand-edited or truncated
// fast_flags block must cost the user their flags at worst, never the rest of
// their settings.
std::vector<FastFlag> fastFlagsFromJson(const nlohmann::json& j) {
    std::vector<FastFlag> flags;
    if (!j.is_array()) return flags;
    for (const auto& entry : j) {
        if (!entry.is_object()) continue;
        FastFlag flag;
        flag.name = entry.value("name", std::string());
        flag.value = entry.value("value", std::string());
        if (flag.name.empty()) continue;
        flags.push_back(std::move(flag));
    }
    return flags;
}

// find() plus a type check, not value(): value() throws when the stored type
// cannot convert to the default's, so a hand-edited "fast_flags": "nonsense"
// would escape to the catch-all below and reset every other setting with it.
std::vector<FastFlag> fastFlagsForTarget(const nlohmann::json& parent, const char* key) {
    if (!parent.is_object()) return {};
    const auto it = parent.find(key);
    if (it == parent.end()) return {};
    return fastFlagsFromJson(*it);
}

nlohmann::json fastFlagsToJson(const std::vector<FastFlag>& flags) {
    nlohmann::json array = nlohmann::json::array();
    for (const auto& flag : flags) {
        if (flag.name.empty()) continue;
        array.push_back({{"name", flag.name}, {"value", flag.value}});
    }
    return array;
}

} // namespace

Settings loadSettings(const std::string& installDir) {
    Settings settings;
    try {
        std::ifstream file(settingsFilePath(installDir));
        if (!file) return settings;

        nlohmann::json j;
        file >> j;

        settings.envVars = j.contains("env_vars") ? j.at("env_vars").get<std::string>()
                                                   : mergeLegacyEnvVars(j);
        settings.sendCrashReports = j.at("send_crash_reports").get<bool>();
        // Read leniently (.value(), not .at()): unlike the three fields
        // above, a settings.json written before this field existed must
        // NOT wholesale-reset to defaults just because "channel" is
        // missing -- it should just default to "stable" and keep whatever
        // else was already there.
        std::string channel = j.value("channel", std::string("stable"));
        settings.channel = isKnownChannel(channel) ? channel : "stable";
        // Read leniently, same reasoning as "channel" above.
        settings.autoUpdate = j.value("auto_update", false);

        const auto fastFlags = j.find("fast_flags");
        if (fastFlags != j.end()) {
            settings.fastFlags.player = fastFlagsForTarget(*fastFlags, "player");
            settings.fastFlags.studio = fastFlagsForTarget(*fastFlags, "studio");
        }
        return settings;
    } catch (...) {
        // Missing file, unreadable file, parse error, or a missing/wrong-typed
        // field -- fall back to defaults wholesale rather than partially
        // applying whatever did parse. A corrupt settings file must never
        // crash the launcher.
        return Settings{};
    }
}

void saveSettings(const std::string& installDir, const Settings& settings) {
    try {
        std::error_code ec;
        fs::create_directories(installDir, ec);

        nlohmann::json j;
        j["env_vars"] = settings.envVars;
        j["send_crash_reports"] = settings.sendCrashReports;
        j["channel"] = settings.channel;
        j["auto_update"] = settings.autoUpdate;
        j["fast_flags"] = {{"player", fastFlagsToJson(settings.fastFlags.player)},
                            {"studio", fastFlagsToJson(settings.fastFlags.studio)}};

        std::ofstream file(settingsFilePath(installDir), std::ios::binary);
        if (!file) return;
        file << j.dump(2);
    } catch (...) {
        // Best-effort -- a failed settings save must not crash the launcher.
    }
}

std::vector<std::string> parseEnvPairs(const std::string& text) {
    std::vector<std::string> pairs;
    std::istringstream stream(text);
    std::string token;
    while (stream >> token) {
        if (token.find('=') != std::string::npos) {
            pairs.push_back(token);
        }
    }
    return pairs;
}

} // namespace tuxblox
