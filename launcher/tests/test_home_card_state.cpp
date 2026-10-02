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

    // Nothing installed: the card offers the install, whatever else is true.
    CardState fresh = cardState(LaunchTarget::Player, false, 0, false);
    assert(fresh.launchLabel == "Install & Launch");
    assert(!fresh.launchStops);
    assert(fresh.stopLabel.empty());
    assert(fresh.sessionLabel.empty());

    // Installed and idle.
    CardState idlePlayer = cardState(LaunchTarget::Player, true, 0, false);
    assert(idlePlayer.launchLabel == "Launch Player");
    assert(!idlePlayer.launchStops);
    assert(idlePlayer.stopLabel.empty());
    assert(idlePlayer.sessionLabel.empty());

    CardState idleStudio = cardState(LaunchTarget::Studio, true, 0, false);
    assert(idleStudio.launchLabel == "Launch Studio");
    assert(idleStudio.stopLabel.empty());

    // The Player's one button flips: one session at a time, so there is nothing
    // for a second button to do.
    CardState livePlayer = cardState(LaunchTarget::Player, true, 1, false);
    assert(livePlayer.launchLabel == "Stop Player");
    assert(livePlayer.launchStops);
    assert(livePlayer.stopLabel.empty());
    assert(livePlayer.sessionLabel == "1 session running");

    CardState stoppingPlayer = cardState(LaunchTarget::Player, true, 1, true);
    assert(stoppingPlayer.launchLabel == "Stopping\xE2\x80\xA6");
    assert(stoppingPlayer.launchStops);

    // Studio keeps its Launch button and gains a second one.
    CardState oneStudio = cardState(LaunchTarget::Studio, true, 1, false);
    assert(oneStudio.launchLabel == "Launch Studio");
    assert(!oneStudio.launchStops);
    assert(oneStudio.stopLabel == "Stop Studio");
    assert(oneStudio.sessionLabel == "1 session running");

    // The singular/plural and count boundaries.
    CardState twoStudio = cardState(LaunchTarget::Studio, true, 2, false);
    assert(twoStudio.stopLabel == "Stop 2");
    assert(twoStudio.sessionLabel == "2 sessions running");

    CardState manyStudio = cardState(LaunchTarget::Studio, true, 12, false);
    assert(manyStudio.stopLabel == "Stop 12");
    assert(manyStudio.sessionLabel == "12 sessions running");

    CardState stoppingStudio = cardState(LaunchTarget::Studio, true, 3, true);
    assert(stoppingStudio.stopLabel == "Stopping\xE2\x80\xA6");
    assert(stoppingStudio.launchLabel == "Launch Studio");

    // A session running against a card that is somehow not installed still
    // offers to stop it -- the version could have been deleted mid-session.
    CardState orphan = cardState(LaunchTarget::Studio, false, 1, false);
    assert(orphan.launchLabel == "Install & Launch");
    assert(orphan.stopLabel == "Stop Studio");
    assert(orphan.sessionLabel == "1 session running");

    std::printf("home_card_state: all tests passed\n");
    return 0;
}
