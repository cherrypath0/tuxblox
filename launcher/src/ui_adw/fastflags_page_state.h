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
#include <cstddef>
#include <string>
#include <vector>

#include "settings.h"

namespace tuxblox {

// Which rows a search term leaves showing, as indices into `flags` rather than a
// filtered copy: a copy saved back would silently drop every flag the term hid.
// Matches the flag's name only, ignoring case; an empty term returns every index.
// Kept apart from the FastFlags page so it can be tested without a display.
std::vector<size_t> visibleFlagIndices(const std::vector<FastFlag>& flags, const std::string& term);

// How many flags are set, which is what the Player/Studio toggle shows.
size_t flagCount(const std::vector<FastFlag>& flags);

} // namespace tuxblox
