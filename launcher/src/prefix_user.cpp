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

#include "prefix_user.h"

#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

const std::string FallbackName = "user";

} // namespace

std::string driveCDir(const std::string& installDir) {
    return installDir + "/runtime/pfx/drive_c";
}

std::string prefixUserName(const std::string& driveCDir) {
    std::error_code error;
    fs::directory_iterator entries(fs::path(driveCDir) / "users", error);
    if (error) return FallbackName;

    std::string found;
    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name == "Public" || !entry.is_directory(error) || error) continue;
        if (!found.empty()) return FallbackName; // two of them; don't guess
        found = name;
    }
    return found.empty() ? FallbackName : found;
}

std::string prefixUserDir(const std::string& installDir) {
    const std::string driveC = driveCDir(installDir);
    return driveC + "/users/" + prefixUserName(driveC);
}

} // namespace tuxblox
