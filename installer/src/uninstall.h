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
#include <string>

namespace tuxblox {

// Deletes every tuxblox-*.desktop file directly under `appsDir` (e.g.
// ~/.local/share/applications) -- tuxblox-launcher.desktop,
// tuxblox-url-handler.desktop, and any other tuxblox-prefixed entry left
// behind by an older version. Best-effort per file: a stray permissions
// error on one entry doesn't stop the rest from being removed.
void removeDesktopFiles(const std::string& appsDir);

// Removes the single file at `iconPath` (e.g.
// ~/.local/share/icons/hicolor/256x256/apps/tuxblox.png -- see
// desktop_integration.cpp/desktop_shortcut.cpp for why it lives under the
// icon theme directory rather than under installDir). Best-effort: fine if
// it never existed.
void removeIconFile(const std::string& iconPath);

// Rewrites the mimeapps.list at `mimeappsPath`, dropping every line that
// references tuxblox-url-handler.desktop (both `[Default Applications]` and
// `[Added Associations]` sections use plain `mimetype=desktop-file[;...]`
// lines, so a substring match on the filename is sufficient without a full
// INI parser). No-op if the file doesn't exist.
void stripMimeappsAssociations(const std::string& mimeappsPath);

// Recursively deletes `installDir` (e.g. ~/.tuxblox). Returns false only if
// the directory existed and could not be fully removed (e.g. a permissions
// error) -- a directory that didn't exist to begin with counts as success.
bool removeInstallDir(const std::string& installDir);

// Full uninstall against the real environment: removes both
// ~/.local/share/applications/tuxblox-*.desktop entries and their
// mimeapps.list associations (checking both the modern
// ~/.config/mimeapps.list and the legacy
// ~/.local/share/applications/mimeapps.list locations), asks the desktop
// environment to refresh its cache, then deletes ~/.tuxblox. No UI of its
// own -- the caller (main.cpp) reports success/failure, same as every other
// popup this binary shows. Returns true on full success.
bool performUninstall();

} // namespace tuxblox
