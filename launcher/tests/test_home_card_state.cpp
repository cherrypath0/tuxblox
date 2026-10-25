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

#include "ui_adw/home_card_state.h"

#include <cassert>
#include <cstdio>

int main() {
    using namespace tuxblox;

    // A launch that has been asked for but has not produced a session yet -- which is most of an
    // install, since Roblox has to be fetched first. The button has to say so and stay unclickable,
    // or an install that takes minutes looks like a button that did nothing.
    {
        CardState starting = cardState(LaunchTarget::Studio, false, 0, false, true);
        assert(starting.launchLabel == "Starting\xE2\x80\xA6");
        assert(starting.launchDisabled);
        assert(!starting.launchStops);
    }
    {
        // Once its session exists the button goes back to what it was.
        CardState live = cardState(LaunchTarget::Studio, true, 1, false, false);
        assert(!live.launchDisabled);
    }
    {
        // The Player's button still turns into Stop while one is pending for the other app.
        CardState player = cardState(LaunchTarget::Player, true, 1, false, false);
        assert(player.launchStops);
    }

    // Nothing installed: the card offers the install, whatever else is true.
    CardState fresh = cardState(LaunchTarget::Player, false, 0, false, false);
    assert(fresh.launchLabel == "Install & Launch");
    assert(!fresh.launchStops);

    // Installed and idle.
    CardState idlePlayer = cardState(LaunchTarget::Player, true, 0, false, false);
    assert(idlePlayer.launchLabel == "Launch Player");
    assert(!idlePlayer.launchStops);

    CardState idleStudio = cardState(LaunchTarget::Studio, true, 0, false, false);
    assert(idleStudio.launchLabel == "Launch Studio");
    assert(!idleStudio.launchStops);

    // The Player's one button flips: one session at a time, so starting another is not on offer.
    CardState livePlayer = cardState(LaunchTarget::Player, true, 1, false, false);
    assert(livePlayer.launchLabel == "Stop Player");
    assert(livePlayer.launchStops);

    CardState stoppingPlayer = cardState(LaunchTarget::Player, true, 1, true, false);
    assert(stoppingPlayer.launchLabel == "Stopping\xE2\x80\xA6");
    assert(stoppingPlayer.launchStops);

    // Studio's card says the same thing however many are open: what is open, and
    // stopping one of them, is the list below the cards rather than the card.
    for (int sessions : {1, 2, 12}) {
        CardState studio = cardState(LaunchTarget::Studio, true, sessions, false, false);
        assert(studio.launchLabel == "Launch Studio");
        assert(!studio.launchStops);
        assert(!studio.launchDisabled);
    }

    CardState stoppingStudio = cardState(LaunchTarget::Studio, true, 3, true, false);
    assert(stoppingStudio.launchLabel == "Launch Studio");
    assert(!stoppingStudio.launchStops);

    // A session running against a card that is somehow not installed still offers
    // the install -- the version could have been deleted mid-session.
    CardState orphan = cardState(LaunchTarget::Studio, false, 1, false, false);
    assert(orphan.launchLabel == "Install & Launch");
    assert(!orphan.launchStops);

    std::printf("home_card_state: all tests passed\n");
    return 0;
}
