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
#include <set>
#include <string>
#include <vector>

namespace tuxblox {

// Where the Roblox client is actually connected, asked of the system rather than read out of a log. This is the one answer a game cannot influence: it can print whatever it likes into Roblox's log, but it cannot change which machine its own process is talking to.
std::string clientPeerAddress(const std::string& prefixDir);

// An address as /proc writes one: four bytes of hex, least significant first. Empty for anything else.
std::string ipv4FromProcHex(const std::string& hex);

// The peers of the given sockets in a /proc/net/udp or /proc/net/tcp table, keeping only addresses worth asking a region of.
std::vector<std::string> routablePeersFromProcNet(const std::string& table,
                                                  const std::set<std::string>& inodes);

// The socket a /proc/<pid>/fd entry points at, or empty when it points at something else.
std::string socketInodeFromLink(const std::string& target);

// Is this the name the kernel gives a Roblox client? Both are cut to fifteen characters there.
bool isRobloxClientComm(const std::string& comm);

// Does this process's environment say it belongs to the virtual drive we launched, rather than to another session of the same program?
bool environNamesPrefix(const std::string& environ, const std::string& prefixDir);

} // namespace tuxblox
