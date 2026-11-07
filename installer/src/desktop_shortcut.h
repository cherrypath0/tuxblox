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
// pointing at launcherExePath, so the launcher is findable via the desktop
// environment's app search. launcherExePath is the just-installed
// TuxBloxLauncher binary's full path -- the same value refreshUrlHandlers()
// takes, i.e. InstallOutcome::launcherPath. It is deliberately NOT derived
// from the install directory: the launcher now ships as an archive artifact
// (a bundle directory), so its executable no longer sits directly at
// installDir. Nothing else here is relative to the install directory -- the
// icon goes to the per-user icon theme -- so this is the function's only
// path input. Best-effort: never throws -- failure here must not fail an
// otherwise-successful install.
void createDesktopShortcut(const std::string& launcherExePath);

// Removes any existing non-development TuxBlox URL-scheme handlers (the
// installed "TuxBlox"/"TuxBlox Player"/"TuxBlox Studio" .desktop entries,
// plus the pre-rename shared handler name from older installs) and writes
// them fresh.
//
// It does NOT make any of them the default for roblox:, roblox-player:,
// roblox-studio: or roblox-studio-auth:. It used to, on every install AND
// every update, which took the association back from whatever the user had
// chosen -- and did it before any Roblox app existed to answer the link.
// Claiming is the launcher's job now, in ensureDesktopIntegration(), and only
// once the app a scheme belongs to is actually installed in the virtual drive.
// Writing the entries here is still needed: the launcher can only make one the
// default if it is already on disk. launcherExePath should be the
// just-installed TuxBloxLauncher binary's full path. Best-effort: never throws
// -- failure here must not fail an otherwise-successful install/update.
void refreshUrlHandlers(const std::string& launcherExePath);

} // namespace tuxblox
