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

// Windows path of the bridge root inside the prefix.
extern const char* const kBridgeWindowsRoot;

// Makes a host file reachable from inside the prefix and returns the Windows
// path Studio should be given, or "" if it can't be reached.
//
// plan/plan.txt item 20 removed the Z: drive (mapping the host root as a drive
// letter is one of the most common Wine-detection heuristics), so only c: exists
// in dosdevices/ and a file under /home is otherwise unreachable from inside the
// prefix. This bridges one file by symlinking its CONTAINING DIRECTORY into
// C:\users\user\Documents\TuxBlox Files.
//
// Directory-level rather than file-level is the whole point: Studio's save path
// is very likely write-temp-then-rename, and renaming over a FILE symlink
// replaces the link with a regular file, silently orphaning the user's original.
// A rename inside a symlinked DIRECTORY lands on the real filesystem.
//
// INVARIANT: the return value is a "C:\..." path or "". It must never be a host
// path -- fake_leaked_command_line() in
// ProtonSource/wine/dlls/kernelbase/process.c appends the file argument to
// Roblox's command line verbatim, so a host path here would leak straight
// through the rewrite it exists to prevent.
std::string bridgeHostPathIntoPrefix(const std::string& installDir, const std::string& hostPath);

// Testable seam: turns a host directory basename into a usable Windows
// directory name. Never returns "".
std::string sanitizeBridgeLinkName(const std::string& name);

} // namespace tuxblox
