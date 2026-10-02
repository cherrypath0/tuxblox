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

// What the system lists this process as -- `ps`, `top`, `pgrep` and a desktop
// system monitor all read this name.
//
// The launcher and the process that watches a Roblox session are the same
// binary, so without renaming one of them they are indistinguishable in a
// process list, and anything matching the launcher by name matches the watcher
// too. Linux keeps the name in 16 bytes, so anything longer is truncated to 15
// characters; false means the system refused the change outright.
bool setProcessName(const std::string& name);

// The name this process is currently listed under.
std::string currentProcessName();

} // namespace tuxblox
