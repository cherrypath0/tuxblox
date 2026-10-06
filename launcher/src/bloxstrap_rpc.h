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

// The message format this implements is BloxstrapRPC, as used by Bloxstrap (MIT, Copyright (c) 2022 pizzaboxer, https://github.com/bloxstraplabs/bloxstrap). Its notice is in third_party_licenses/bloxstrap/. Nothing is copied from it -- the behaviour is matched so that a game setting its status reaches a player the same way it does on Windows.

#pragma once
#include <string>

#include "presence.h"

namespace tuxblox {

// Does this log line carry a message from the game at all?
bool isBloxstrapRpcLine(const std::string& line);

// Applies one message onto an override. False means nothing was honoured and the override is untouched, which is the answer for every malformed, oversized or unrecognised line.
bool applyBloxstrapRpcLine(const std::string& line, RpcOverride& into);

// How much launch data Roblox will carry, which is what the protocol's other command sets.
const std::size_t MaxLaunchDataLength = 200;

// Where Roblox describes the asset a game named. Empty for anything that is not plain digits, because a game must not be able to choose the picture that appears on somebody's profile. This answers with a description, not a picture, so what comes back has to be read before Discord can be given anything.
std::string rpcAssetLookupUrl(const std::string& assetId);

} // namespace tuxblox
