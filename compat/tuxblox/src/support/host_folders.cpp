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

#include "host_folders.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace tuxblox {

namespace {

// Strips surrounding double quotes and any trailing separators.
std::string unquote(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        value = value.substr(1, value.size() - 2);
    }
    while (value.size() > 1 && value.back() == '/') value.pop_back();
    return value;
}

// Reads one key out of the desktop's user-dirs.dirs. Returns "" when the file
// or the key is missing.
std::string readUserDirsValue(const std::string& home, const std::string& key) {
    std::ifstream in(std::filesystem::path(home) / ".config" / "user-dirs.dirs");
    if (!in) return "";

    const std::string prefix = key + "=";
    std::string line;
    std::string found;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] == '#') continue;
        if (line.rfind(prefix, 0) != 0) continue;
        found = unquote(line.substr(prefix.size()));
    }
    return found;
}

} // namespace

const std::vector<HostFolder>& hostFolderMappings() {
    static const std::vector<HostFolder> Folders = {
        {"Desktop", "XDG_DESKTOP_DIR"},
        {"Documents", "XDG_DOCUMENTS_DIR"},
        {"Downloads", "XDG_DOWNLOAD_DIR"},
        {"Music", "XDG_MUSIC_DIR"},
        {"Pictures", "XDG_PICTURES_DIR"},
        {"Videos", "XDG_VIDEOS_DIR"},
    };
    return Folders;
}

std::string hostFolderPath(const std::string& xdgKey, const std::string& englishName,
                           const std::string& home) {
    if (home.empty()) return "";

    std::string value = readUserDirsValue(home, xdgKey);
    if (value.empty()) return (std::filesystem::path(home) / englishName).string();

    if (value.rfind("$HOME", 0) == 0) {
        value = home + value.substr(std::string("$HOME").size());
        while (value.size() > 1 && value.back() == '/') value.pop_back();
    }
    if (value.empty() || value[0] != '/') return "";

    // The spec uses the home folder itself for "there is no such folder", and linking it would hand over the whole home rather than one folder.
    if (std::filesystem::path(value) == std::filesystem::path(home)) return "";
    return value;
}

bool removeLegacyBridgeRoot(const std::filesystem::path& documentsFolder) {
    namespace fs = std::filesystem;
    std::error_code error;

    const fs::path old = documentsFolder / "TuxBlox Files";
    if (!fs::exists(fs::symlink_status(old, error))) return true;

    fs::directory_iterator entries(old, error);
    if (error) return false;
    for (const fs::directory_entry& entry : entries) {
        if (!fs::is_symlink(entry.symlink_status(error)) || error) return false;
    }

    fs::remove_all(old, error);
    return !error;
}

HostFolderLink linkHostFolder(const std::filesystem::path& prefixFolder,
                              const std::filesystem::path& hostFolder) {
    namespace fs = std::filesystem;
    std::error_code error;

    if (!fs::is_directory(hostFolder, error)) return HostFolderLink::NoHostFolder;

    const bool isLink = fs::is_symlink(fs::symlink_status(prefixFolder, error));
    if (isLink) {
        if (fs::read_symlink(prefixFolder, error) == hostFolder && !error) {
            return HostFolderLink::AlreadyLinked;
        }
        fs::remove(prefixFolder, error);
    } else if (fs::is_directory(prefixFolder, error)) {
        bool everythingMoved = true;
        fs::directory_iterator entries(prefixFolder, error);
        if (error) return HostFolderLink::Kept;
        for (const fs::directory_entry& entry : entries) {
            // Wine's own empty scaffolding, like the nested Templates and My Music folders. There is nothing in them to keep, and moving them would litter the user's real folder.
            std::error_code emptyError;
            if (entry.is_directory(emptyError) && !emptyError &&
                fs::is_empty(entry.path(), emptyError) && !emptyError) {
                std::error_code removeError;
                fs::remove(entry.path(), removeError);
                if (removeError) everythingMoved = false;
                continue;
            }

            const fs::path destination = hostFolder / entry.path().filename();
            // Never write over something of the user's that happens to share a name.
            if (fs::exists(fs::symlink_status(destination, error))) {
                everythingMoved = false;
                continue;
            }
            std::error_code moveError;
            fs::rename(entry.path(), destination, moveError);
            if (moveError) everythingMoved = false;
        }
        if (!everythingMoved) return HostFolderLink::Kept;

        fs::remove(prefixFolder, error);
        if (error) return HostFolderLink::Kept;
    }

    fs::create_directory_symlink(hostFolder, prefixFolder, error);
    return error ? HostFolderLink::Kept : HostFolderLink::Linked;
}

} // namespace tuxblox
