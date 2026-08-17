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

// Escapes a desktop-entry id for use inside a systemd unit name, the way
// systemd's own unit-name escaping does: '-' is the path separator in unit
// names, so it has to become "\x2d" or the id can't be recovered from the
// unit name afterward ("tuxblox-launcher" -> "tuxblox\x2dlauncher").
// Exposed for testing.
std::string escapeUnitName(const std::string& desktopId);

// Builds the transient scope unit name for a pid, in the form desktop
// tooling parses back into a desktop-entry id:
//   app-tuxblox\x2dlauncher-1234.scope
// Exposed for testing.
std::string appScopeUnitName(const std::string& desktopId, int pid);

// True if this process is already inside a systemd scope/service named for
// desktopId, i.e. joining another one would be redundant. cgroupLine is the
// contents of /proc/self/cgroup. Exposed for testing.
bool alreadyInAppScope(const std::string& cgroupLine, const std::string& desktopId);

// Best-effort: move this process into a transient systemd scope named after
// the TuxBlox desktop entry, so the desktop can identify it.
//
// Why this exists at all. Desktop shells identify a running application by
// mapping its systemd unit name back to a .desktop file -- KDE's System
// Monitor does exactly this to pick the name and icon for a row. A unit is
// only named after a desktop entry when the *desktop environment* launched
// that entry; anything that starts the binary directly (a terminal, a file
// manager double-click, a shortcut holding the raw path) instead gets a unit
// named after the executable path, e.g.
//   app-\x2fhome\x2fuser\x2f.tuxblox\x2fTuxBloxLauncher@<hash>.service
// which matches no desktop entry, so the process shows up as a bare path
// with no icon -- or is left out of the applications list entirely.
//
// Registering our own correctly-named scope makes that identification work
// regardless of how the launcher was started. Children inherit the cgroup, so
// doing this once in the launcher also covers the detached --watch-launch
// helper and, through it, Proton and Roblox itself -- which matters because
// the helper is what survives for the whole session and it has no window of
// its own for the shell to fall back on.
//
// Entirely best-effort and never fatal: no systemd user session, no busctl,
// or a refused call all just leave the process where it already was.
void ensureAppScope();

} // namespace tuxblox
