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

#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace tuxblox {

// One folder inside the virtual drive that points at the matching folder in
// the user's home, so Roblox can open and save there directly.
struct HostFolder {
    // The folder's name inside the drive, e.g. "Downloads".
    std::string windowsName;
    // The key the desktop records it under, e.g. "XDG_DOWNLOAD_DIR".
    std::string xdgKey;
};

// The folders that get pointed at the host. Wine's own extras -- Contacts,
// Favorites, Links, Saved Games, Searches -- are left alone, since a Linux
// home has nothing matching them.
const std::vector<HostFolder>& hostFolderMappings();

// Where the host keeps that folder, read from the desktop's own
// ~/.config/user-dirs.dirs so a translated home folder is followed rather
// than guessed at. Falls back to `englishName` under the home folder. Returns
// an empty string when there is no home folder to look in, or when the
// desktop records the home folder itself, which the spec uses to mean the
// folder does not exist and which must never be linked.
std::string hostFolderPath(const std::string& xdgKey, const std::string& englishName,
                           const std::string& home);

// What a linkHostFolder() pass did, so the caller can report it.
enum class HostFolderLink {
    Linked,
    // Was already pointing at the right place.
    AlreadyLinked,
    // The home folder has no such folder, so nothing was changed and none was created.
    NoHostFolder,
    // Something in it could not be moved out, so it stays a real folder.
    Kept,
};

// Points a folder in the drive at the matching one in the user's home. Anything
// already in it is moved across first, and a name the home folder already uses
// is never overwritten -- if even one entry has to stay, the folder is left as
// it was rather than hiding those files behind a link.
HostFolderLink linkHostFolder(const std::filesystem::path& prefixFolder,
                              const std::filesystem::path& hostFolder);

// Removes the "TuxBlox Files" folder TuxBlox used to keep inside Documents,
// now that files are bridged into the account's own "files" folder instead.
// Everything in it is a shortcut TuxBlox makes again when it is needed, so it
// is safe to drop -- but only if that is all it holds. True when the folder is
// gone, including when it was never there.
bool removeLegacyBridgeRoot(const std::filesystem::path& documentsFolder);

} // namespace tuxblox
