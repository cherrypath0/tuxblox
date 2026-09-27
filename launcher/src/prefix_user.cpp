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

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

const std::string FallbackName = "user";

} // namespace

std::string driveCDir(const std::string& installDir) {
    return installDir + "/runtime/pfx/drive_c";
}

std::string hostAccountName() {
    const char *pUser = std::getenv("USER");
    if (pUser && pUser[0] != '\0') return pUser;

    const struct passwd *pEntry = ::getpwuid(::getuid());
    if (pEntry && pEntry->pw_name && pEntry->pw_name[0] != '\0') return pEntry->pw_name;
    return FallbackName;
}

std::string prefixUserName(const std::string& driveCDir, const std::string& preferred) {
    std::error_code error;
    fs::directory_iterator entries(fs::path(driveCDir) / "users", error);
    if (error) return FallbackName;

    std::vector<std::string> found;
    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name == "Public" || !entry.is_directory(error) || error) continue;
        found.push_back(name);
    }

    if (found.empty()) return FallbackName;
    if (found.size() == 1) return found.front();

    // More than one. Whichever matches this computer's account is the folder %USERPROFILE% names, so it is the one Roblox is really using.
    if (std::find(found.begin(), found.end(), preferred) != found.end()) return preferred;
    return FallbackName;
}

std::string prefixUserName(const std::string& driveCDir) {
    return prefixUserName(driveCDir, hostAccountName());
}

std::string prefixUserDir(const std::string& installDir) {
    const std::string driveC = driveCDir(installDir);
    return driveC + "/users/" + prefixUserName(driveC);
}

} // namespace tuxblox
