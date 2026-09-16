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

namespace tuxblox {

// Multiplier for every pixel size, position and font size in the window, so
// it reads at one physical size on any display. 96 DPI is 1.0, the desktop
// convention. Pass dpi <= 0 when the platform cannot report it and the
// display's pixel height is used against a 1440p baseline instead. The
// result is always clamped to 0.75 - 3.0.
float computeUiScale(float dpi, int displayHeightPx);

} // namespace tuxblox
