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
#include "settings.h"

#include <string>
#include <vector>

namespace tuxblox {

struct FastFlagImport {
    bool ok = false;
    std::vector<FastFlag> flags;
    // Plain words about what is wrong, empty when ok
    std::string error;
};

// Reads {"FlagName": value, ...}, the shape Roblox's own flag files use. Values may be strings, numbers or true/false, and come out as the text a flag file stores.
FastFlagImport parseFastFlagJson(const std::string &text);

// existing with imported laid over it: a flag that is already there takes the new value where it stands, and new ones go on the end
std::vector<FastFlag> mergeFastFlags(const std::vector<FastFlag> &existing, const std::vector<FastFlag> &imported);

} // namespace tuxblox
