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

} // namespace

Settings loadSettings(const std::string& installDir) {
    Settings settings;
    try {
        std::ifstream file(settingsFilePath(installDir));
        if (!file) return settings;

        nlohmann::json j;
        file >> j;

        settings.protonEnvVars = j.at("proton_env_vars").get<std::string>();
        settings.globalEnvVars = j.at("global_env_vars").get<std::string>();
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
        j["proton_env_vars"] = settings.protonEnvVars;
        j["global_env_vars"] = settings.globalEnvVars;
        j["send_crash_reports"] = settings.sendCrashReports;
        j["channel"] = settings.channel;
        j["auto_update"] = settings.autoUpdate;

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
