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

} // namespace tuxblox
