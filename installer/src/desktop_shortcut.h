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

// Writes a desktop icon file and an XDG .desktop entry ("TuxBlox Launcher")
// pointing at installDir + "/TuxBloxLauncher", so the launcher is findable
// via the desktop environment's app search. Best-effort: never throws --
// failure here must not fail an otherwise-successful install.
void createDesktopShortcut(const std::string& installDir);

// Removes any existing non-development TuxBlox URL-scheme handlers (the
// installed "TuxBlox"/"TuxBlox Player"/"TuxBlox Studio" .desktop entries,
// plus the pre-rename shared handler name from older installs) and writes
// them fresh, forcing xdg-mime's default back to them for roblox:,
// roblox-player:, roblox-studio:, and roblox-studio-auth:. Unlike the
// launcher's own ensureDesktopIntegration(), this does NOT skip schemes
// currently pointed at a repo-local dev handler (install-handler.sh) --
// running the installer is a deliberate top-level action and should always
// leave the installed launcher as the real default. launcherExePath should
// be the just-installed TuxBloxLauncher binary's full path. Best-effort:
// never throws -- failure here must not fail an otherwise-successful
// install/update.
void refreshUrlHandlers(const std::string& launcherExePath);

} // namespace tuxblox
