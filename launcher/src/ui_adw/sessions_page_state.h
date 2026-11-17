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
#include <ctime>
#include <string>
#include <vector>

#include "session_list.h"

namespace tuxblox {

// One line in the Sessions list, in the words it is shown with.
struct SessionRow {
    std::string name;
    std::string meta;
    bool running = false;
    // Ended on something other than a clean close, so its mark is red rather than grey.
    bool crashed = false;
    int pid = 0;
};

struct SessionsView {
    std::vector<SessionRow> running;
    std::vector<SessionRow> ended;
    // What to say when there is nothing to list. Empty whenever there is.
    std::string emptyMessage;
};

// Kept apart from the Sessions page so it can be tested without a display.
SessionsView sessionsView(const std::vector<Session>& sessions, std::time_t now);

// The same view with only what matches `query`, ignoring case, across the app's name and the line under it. An empty query keeps everything.
SessionsView filterSessions(const SessionsView& view, const std::string& query);

// Where the log view sits, taken straight from its scroll position.
struct LogScrollState {
    double upper = 0.0;
    double pageSize = 0.0;
    double value = 0.0;
};

// Whether the reader has scrolled away from the end of the log, and so should be left where they
// are rather than pulled along by every line that arrives.
//
// Judged on the move rather than the distance alone, because a burst of lines opens the same gap a
// reader scrolling up does: the range grows under a view that has not caught up to its own end yet,
// and reading that as the reader leaving is what stopped the log following during a flood of them.
// So a move that came with the range changing keeps whatever was decided before it, and only a move
// within a range that stayed put is the reader's. Arriving at the end by any means follows again.
bool readerLeftLogTail(const LogScrollState& before, const LogScrollState& now, bool wasAway);

} // namespace tuxblox
