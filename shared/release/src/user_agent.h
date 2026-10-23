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

// What TuxBlox calls itself on every request it makes.
//
//     TuxBlox{Client,Installer}/{version} ({x86_64,arm64}; {stable,canary,experimental})
//
// Two product names, not one per component: Installer for the installer, Client for everything
// else. It stays unambiguous because only two programs make requests at all -- the launcher and
// the installer -- so the version after the slash always belongs to the program named before it.
//
// Built once at startup rather than assembled at each call site: downloadFile() is a free function
// with no access to settings, and seven places each building their own string is seven places for
// them to drift apart.
namespace tuxblox {

enum class UserAgentProduct { Client, Installer };

// Call once, as early as the channel is known. `channel` may be empty or unrecognised.
void setUserAgent(UserAgentProduct product, const std::string& version, const std::string& channel);

// Never empty. Before setUserAgent() it names the product and nothing it would have to invent.
const std::string& userAgent();

} // namespace tuxblox
