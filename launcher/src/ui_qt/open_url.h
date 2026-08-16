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

// Opens `url` in the user's default browser via xdg-open, without leaving a
// zombie behind -- see the .cpp for the double-fork detach mechanics
// (unchanged from ui.cpp's original openUrl(), just relocated here since
// ui.cpp itself is going away in Task 12).
void openUrl(const char* url);

} // namespace tuxblox
