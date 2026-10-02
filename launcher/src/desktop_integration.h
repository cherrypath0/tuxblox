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
//
// `installDir` is needed for exportPrefixShortcuts(), which reads the prefix's
// own c:\proton_shortcuts -- see wine_shortcut_export.h.
void ensureDesktopIntegration(const std::string& launcherExePath, const std::string& installDir);

// Whether TuxBlox should make itself the default for a file type or a link
// scheme. `currentDefault` is the desktop id the system reports for it now,
// empty when nothing is set; `currentDefaultInstalled` says whether that id
// names a .desktop file that is actually present.
//
// It claims the type when nothing is set, when the default is already one of
// TuxBlox's own installed entries, or when the default names a program that is
// not installed -- a default pointing at something that is gone opens nothing,
// so there is no choice there to respect.
//
// It leaves anything else alone. Somebody running another Roblox program
// alongside TuxBlox chose that, and the launcher runs on every startup, so
// taking the type back each time would make their choice impossible to keep. A
// development handler is left alone for the same reason: it is put there by
// hand and outranks the installed one.
bool shouldClaimAssociation(const std::string& currentDefault, bool currentDefaultInstalled);

// The desktop id explicitly set as the default for `mimeType` in a mimeapps.list,
// given that file's text. Empty when it sets none.
//
// Only an entry naming the exact type counts. Asking the desktop instead
// answers for a type nobody has set, because a Roblox place in XML form is also
// XML, and the XML handler comes back -- which would read as somebody having
// chosen a web browser for Roblox files. Reading the file also avoids
// `xdg-mime query default`, which reports the wrong application entirely on a
// KDE system that has no qtpaths installed.
std::string explicitDefaultFor(const std::string& mimeappsText, const std::string& mimeType);

} // namespace tuxblox
