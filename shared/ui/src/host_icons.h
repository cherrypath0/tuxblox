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

// Where the icons below are written. It is named whether or not any are there, so the caller can offer it to the interface in the same breath as its own icons rather than in a second pass, which costs a rescan.
std::string hostWindowControlsDir();

// Takes the desktop's own minimise, maximise, restore and close icons and writes them where the window's buttons will find them, so TuxBlox's title bar matches the rest of the desktop instead of carrying its own.
// They go into a small icon theme inheriting `fallbackTheme`, so every other icon still comes from the set TuxBlox ships. Returns that theme's name for GtkSettings.
// Returns empty whenever the desktop's own cannot be read -- no theme named, the theme and everything it inherits have none, or one will not render -- which leaves the caller on `fallbackTheme`. All four have to be found together, since three of the desktop's beside one of ours would look worse than four of ours.
std::string buildHostWindowControls(const std::string &fallbackTheme);

} // namespace tuxblox
