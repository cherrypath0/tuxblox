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

#include "presence.h"

namespace tuxblox {

// Does this log line carry a message from the game at all?
bool isBloxstrapRpcLine(const std::string& line);

// Applies one message onto an override. False means nothing was honoured and the override is untouched, which is the answer for every malformed, oversized or unrecognised line.
bool applyBloxstrapRpcLine(const std::string& line, RpcOverride& into);

// Where Roblox serves the image for an asset a game named. Empty for anything that is not plain digits, because a game must not be able to choose the picture that appears on somebody's profile.
std::string rpcAssetImageUrl(const std::string& assetId);

} // namespace tuxblox
