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
#include <optional>
#include <string>
#include <vector>

#include "lnk_resolver.h"

namespace tuxblox {

// One run of Roblox in the virtual drive. A session is identified by its process and when that process started, never by the process alone, because the system hands the same process number out again later.
struct Session {
    LaunchTarget target = LaunchTarget::Player;
    int pid = 0;
    std::time_t startedAt = 0;
    unsigned long long residentBytes = 0;
    bool running = true;
    std::time_t endedAt = 0;
    std::string endedReason;
    std::string capturedLogPath;
    std::string robloxLogPath;
};

// Every Roblox process alive in the drive, oldest first, whoever started it -- a session started from a link before the launcher opened is as real as one started from its buttons.
std::vector<Session> liveSessionsIn(const std::string& procRoot, const std::string& prefixDir);

// How a session that is over ended, in words for the row underneath its name.
std::string describeExit(bool stopRequested, std::optional<int> markerExitCode);

// Carries what was known forward over a fresh census: a session still there keeps everything already learned about it, one that has gone becomes an ended entry, and one that already ended is left exactly as it was. Running first, oldest first; then ended, most recent first.
std::vector<Session> mergeSessions(const std::vector<Session>& previous,
                                   const std::vector<Session>& live);

} // namespace tuxblox
