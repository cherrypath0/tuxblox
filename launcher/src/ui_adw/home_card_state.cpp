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

#include "home_card_state.h"

namespace tuxblox {

namespace {

const char Stopping[] = "Stopping\xE2\x80\xA6";

std::string sessionCount(int sessions) {
    if (sessions <= 0) return "";
    return std::to_string(sessions) + (sessions == 1 ? " session running" : " sessions running");
}

} // namespace

CardState cardState(LaunchTarget target, bool installed, int sessions, bool stopping) {
    CardState state;
    // Studio only: the Player runs one session at a time, so a count says nothing its Stop button does not.
    state.sessionLabel = target == LaunchTarget::Studio ? sessionCount(sessions) : "";

    const bool playerRunning = target == LaunchTarget::Player && sessions > 0;
    if (playerRunning) {
        state.launchLabel = stopping ? Stopping : "Stop Player";
        state.launchStops = true;
        return state;
    }

    if (!installed) {
        state.launchLabel = "Install & Launch";
    } else {
        state.launchLabel = target == LaunchTarget::Player ? "Launch Player" : "Launch Studio";
    }

    if (target == LaunchTarget::Studio && sessions > 0) {
        if (stopping) {
            state.stopLabel = Stopping;
        } else {
            // No app name: it sits beside Launch under a heading that already says which app this is, and the room it saves is room the Launch label would otherwise lose.
            state.stopLabel = sessions == 1 ? "Stop" : "Stop " + std::to_string(sessions);
        }
    }
    return state;
}

} // namespace tuxblox
