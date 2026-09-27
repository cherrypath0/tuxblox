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
#include <fstream>
#include <sstream>

#include <pwd.h>
#include <unistd.h>

namespace tuxblox {

namespace {

// A Windows account name cannot be longer than this, so a longer host name is not something this drive could report truthfully.
const std::size_t MaxAccountNameLength = 20;

const std::array<std::string, 23> ReservedNames = {
    // Public is the drive's own shared folder, so an account of that name would collide with it and nothing would find Roblox.
    "PUBLIC",
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

// Replaces every occurrence of a literal in a text file. Writes a new file and renames it over the old one, so a write that fails part way leaves the original intact rather than truncated. False means the file still holds the old text.
bool replaceTextInFile(const std::filesystem::path& file, const std::string& from,
                       const std::string& to) {
    namespace fs = std::filesystem;

    std::ifstream in(file);
    if (!in) return true; // nothing there to rewrite
    std::stringstream buffer;
    buffer << in.rdbuf();
    in.close();

    std::string text = buffer.str();
    std::size_t at = text.find(from);
    if (at == std::string::npos) return true;
    while (at != std::string::npos) {
        text.replace(at, from.size(), to);
        at = text.find(from, at + to.size());
    }

    const fs::path temp = fs::path(file).concat(".tuxblox-new");
    {
        std::ofstream out(temp, std::ios::trunc | std::ios::binary);
        if (!out) return false;
        out << text;
        out.flush();
        if (!out) {
            out.close();
            std::error_code ignored;
            fs::remove(temp, ignored);
            return false;
        }
    }

    std::error_code error;
    fs::rename(temp, file, error);
    if (error) {
        std::error_code ignored;
        fs::remove(temp, ignored);
        return false;
    }
    return true;
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

AccountFolderMigration migrateAccountFolder(const std::filesystem::path& prefixDir,
                                            const std::string& wanted) {
    namespace fs = std::filesystem;

    if (wanted == FallbackName) return AccountFolderMigration::NotNeeded;

    const fs::path users = prefixDir / "drive_c/users";
    const fs::path oldDir = users / FallbackName;
    const fs::path newDir = users / wanted;

    std::error_code error;
    const bool haveOld = fs::exists(oldDir, error);
    const bool haveNew = fs::exists(newDir, error);

    if (haveNew) return haveOld ? AccountFolderMigration::Ambiguous : AccountFolderMigration::NotNeeded;
    if (!haveOld) return AccountFolderMigration::NotNeeded;

    fs::rename(oldDir, newDir, error);
    if (error) return AccountFolderMigration::Failed;

    // The drive's own settings record the old path in several places, so they are rewritten in the same pass rather than left pointing at nothing.
    bool registryComplete = true;
    for (const char *pFile : {"user.reg", "system.reg", "userdef.reg"}) {
        registryComplete &= replaceTextInFile(prefixDir / pFile,
                                              "C:\\\\users\\\\" + FallbackName,
                                              "C:\\\\users\\\\" + wanted);
    }
    return registryComplete ? AccountFolderMigration::Renamed
                            : AccountFolderMigration::RegistryIncomplete;
}

std::string retargetAfterAccountRename(const std::string& path,
                                       const std::filesystem::path& prefixDir,
                                       const std::string& account) {
    namespace fs = std::filesystem;

    if (account == FallbackName) return path;

    const std::string oldBase = (prefixDir / "drive_c/users" / FallbackName).string() + "/";
    if (path.rfind(oldBase, 0) != 0) return path;

    std::error_code error;
    if (fs::exists(path, error)) return path;

    const std::string fixed =
        (prefixDir / "drive_c/users" / account).string() + "/" + path.substr(oldBase.size());
    return fs::exists(fixed, error) ? fixed : path;
}

} // namespace tuxblox
