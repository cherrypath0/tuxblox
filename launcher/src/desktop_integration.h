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

// The fast, synchronous half of desktop integration: writes the XDG icon
// file and .desktop entries (main entry + URL-scheme handlers), nothing
// else. Call this BEFORE the main window is shown -- desktop
// environments/compositors that resolve a new window's icon by matching its
// app_id/WM_CLASS against an installed .desktop file do that resolution
// once, at window-creation time, and don't retry later. Calling this only
// from ensureDesktopIntegration() (which runs AFTER show(), to avoid
// blocking the window's appearance on the slow xdg-mime/database calls
// below) left a real race: the window would appear and get tracked by the
// window manager/taskbar before its .desktop file existed, so the very
// first icon lookup failed and never got retried -- observed for real on
// KDE Plasma/KWin, first-run window and taskbar icon staying blank forever
// even though the .desktop file appeared correctly moments later. Never
// throws; a failure here just means the app runs with the OS's generic
// icon instead of TuxBlox's, not a functional problem.
void writeDesktopEntries(const std::string& launcherExePath);

// The rest of desktop integration: re-writes the same entries
// writeDesktopEntries() does (idempotent, cheap, kept here too so this
// function is still fully self-sufficient if ever called on its own), then
// runs the slow, best-effort parts -- xdg-mime default-handler
// registration, update-desktop-database, gtk-update-icon-cache, and (inside
// a Distrobox container) distrobox-export. These affect MIME-type
// association and icon-cache freshness, not the initial icon lookup
// writeDesktopEntries() above exists to win the race for -- safe to run
// after the window is already shown, per the ~12s-worst-case timing this
// ordering was already built around.
void ensureDesktopIntegration(const std::string& launcherExePath);

} // namespace tuxblox
