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

#include "lnk_resolver.h"

namespace tuxblox {

// What one Home card's buttons say. The Player's single button turns into Stop
// because only one Player session runs at a time; Studio keeps Launch and gains
// a second button, since several Studio sessions can be open at once.
struct CardState {
    std::string launchLabel;
    // The main button stops the session rather than starting one, so it is red.
    bool launchStops = false;
    // Studio's separate Stop button, hidden while this is empty.
    std::string stopLabel;
    // "2 sessions running" on Studio, always empty on the Player, which runs one session at a time.
    std::string sessionLabel;
};

// Kept apart from the Home page so it can be tested without a display.
CardState cardState(LaunchTarget target, bool installed, int sessions, bool stopping);

} // namespace tuxblox
