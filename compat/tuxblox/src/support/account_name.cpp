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

#include "account_name.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdlib>

#include <pwd.h>
#include <unistd.h>

namespace tuxblox {

namespace {

// A Windows account name cannot be longer than this, so a longer host name is not something this drive could report truthfully.
const std::size_t MaxAccountNameLength = 20;

const std::array<std::string, 23> ReservedNames = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9",
};

const std::string FallbackName = "user";

std::string toUpper(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return text;
}

bool isUsable(const std::string& name) {
    if (name.empty() || name.size() > MaxAccountNameLength) return false;
    if (name == "." || name == "..") return false;
    if (name.back() == '.' || name.back() == ' ') return false;

    for (const char c : name) {
        if (static_cast<unsigned char>(c) < ' ') return false;
        if (std::string("\\/:*?\"<>|").find(c) != std::string::npos) return false;
    }

    const std::string upper = toUpper(name);
    return std::find(ReservedNames.begin(), ReservedNames.end(), upper) == ReservedNames.end();
}

} // namespace

std::string sanitizedAccountName(const std::string& raw) {
    return isUsable(raw) ? raw : FallbackName;
}

std::string accountName() {
    const char *pUser = std::getenv("USER");
    if (pUser && pUser[0] != '\0') return sanitizedAccountName(pUser);

    const struct passwd *pEntry = ::getpwuid(::getuid());
    if (pEntry && pEntry->pw_name && pEntry->pw_name[0] != '\0') {
        return sanitizedAccountName(pEntry->pw_name);
    }
    return FallbackName;
}

std::string prefixAccountName(const std::filesystem::path& prefixDir) {
    namespace fs = std::filesystem;

    std::error_code error;
    fs::directory_iterator entries(prefixDir / "drive_c/users", error);
    if (error) return FallbackName;

    std::string found;
    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name == "Public" || !entry.is_directory(error) || error) continue;
        if (!found.empty()) return ""; // two account folders; the caller says so
        found = name;
    }
    return found.empty() ? FallbackName : found;
}

} // namespace tuxblox
