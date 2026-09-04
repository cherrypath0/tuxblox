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

#include "fastflag_file.h"
#include "json.hpp"
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tuxblox {

std::string renderClientAppSettings(const std::vector<FastFlag>& flags) {
    // ordered_json, not json: the plain type sorts its keys, which would
    // reshuffle the file every time the user reorders rows in the editor.
    nlohmann::ordered_json j = nlohmann::ordered_json::object();
    for (const auto& flag : flags) {
        if (flag.name.empty()) continue; // a half-typed row is not a flag
        j[flag.name] = flag.value;
    }
    return j.dump(2);
}

bool writeClientAppSettings(const std::string& versionDir, const std::vector<FastFlag>& flags) {
    const fs::path settingsDir = fs::path(versionDir) / kClientSettingsDirName;
    const fs::path filePath = settingsDir / kClientAppSettingsFileName;
    std::error_code ec;

    bool anyNamed = false;
    for (const auto& flag : flags) {
        if (!flag.name.empty()) { anyNamed = true; break; }
    }

    if (!anyNamed) {
        // remove() reports "there was nothing to remove" as success with no
        // error code, which is the outcome we want for an already-clean
        // version directory.
        fs::remove(filePath, ec);
        return !ec;
    }

    fs::create_directories(settingsDir, ec);
    if (ec) return false;

    std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file << renderClientAppSettings(flags);
    return static_cast<bool>(file);
}

} // namespace tuxblox
