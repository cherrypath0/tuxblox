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

namespace tuxblox {

const char* darkPaletteCss();
const char* lightPaletteCss();
const char* paletteStylesCss();

// The value `pName` is given in `pCss`, or "" when that palette does not set it.
std::string paletteColour(const char* pCss, const char* pName);

// How far apart two colours are to read, on the 1.0 to 21.0 scale the accessibility guidelines measure in. Zero when either one is not a six-digit hex colour, so a caller cannot mistake an unreadable answer for a passing one.
double contrastRatio(const std::string& hexA, const std::string& hexB);

} // namespace tuxblox
