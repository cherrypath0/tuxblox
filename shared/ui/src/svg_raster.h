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
#include <vector>

namespace tuxblox {

// Draws `pSvg` at `pixelSize` square as RGBA, with the colour discarded and only the coverage left in the alpha channel, which is the form the interface tints to match the text around it.
// False, leaving `outRgba` alone, for empty or unparseable markup and for a size that is not positive.
bool rasteriseSvgMask(const char* pSvg, size_t length, int pixelSize, std::vector<unsigned char>& outRgba);

} // namespace tuxblox
