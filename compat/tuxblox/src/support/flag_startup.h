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
#include "FlagManager.h"

namespace tuxblox {

// Loads the flags for a layer run. The launcher passes TUXBLOX_FLAGS_CHANNEL and
// TUXBLOX_FLAGS_RELEASED in the child environment, because the layer knows nothing of
// its own context beyond the prefix it was given.
//
// Neither variable set is the ordinary state of a layer run started by hand -- `main run
// something.exe` out of build/ -- and it means no request. That matters here more than
// anywhere else: this sits on the path between pressing Play and Roblox starting.
void initLayerFlags(const std::string& installDir, FlagFetch fetch = {});

} // namespace tuxblox
