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

// A log Roblox wrote, as a possible match for a session.
struct LogCandidate {
    std::string path;
    LaunchTarget target = LaunchTarget::Player;
    // When the log says it began. Taken from the name rather than the file's modification time, which keeps moving while the log is still being written.
    std::time_t startedAt = 0;
};

// True for a log Roblox's crash reporter wrote. It carries the same marker the client's own log does, so without this it is eligible to be shown as a session's log.
bool isCrashHandlerLog(const std::string& filename);

// When a Roblox log's own name says it began, or zero if the name does not carry one.
std::time_t robloxLogStartTime(const std::string& filename);

// One log for each session, in the order the sessions came in, or "" where nothing fits. A log is never given to a session that started after it, never to the wrong app, and never to two sessions.
std::vector<std::string> matchSessionLogs(const std::vector<LogCandidate>& candidates,
                                          const std::vector<Session>& sessions);

} // namespace tuxblox
