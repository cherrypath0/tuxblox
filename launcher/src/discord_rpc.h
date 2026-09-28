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
#include <cstdint>
#include <string>
#include <vector>

namespace tuxblox {

// Every place Discord may put its socket on Linux, in the order they are tried: the plain one, Flatpak's, then Snap's.
std::vector<std::string> discordSocketCandidates(const std::string& runtimeDir);

// Discord's IPC frame: a little-endian opcode and length, then the JSON body.
std::string encodeFrame(uint32_t opcode, const std::string& payload);

} // namespace tuxblox
