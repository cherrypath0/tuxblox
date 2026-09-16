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

std::string bridgeHostPathIntoPrefix(const std::string& installDir, const std::string& hostPath);

// Testable seam: turns a host directory basename into a usable Windows
// directory name. Never returns "".
std::string sanitizeBridgeLinkName(const std::string& name);

// Testable seam: true when `parent` IS the filesystem root (not merely
// somewhere under it). bridgeHostPathIntoPrefix() refuses to bridge a file
// whose parent is the root -- see its own comment for why. Exposed as a
// pure check (no filesystem access) so that refusal can be unit tested
// without needing write access to the real "/".
bool isFilesystemRoot(const std::string& parent);

} // namespace tuxblox
