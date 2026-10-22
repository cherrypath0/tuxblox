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

#include "session_list.h"

#include "prefix_session.h"
#include "process_launcher.h"
#include "session_facts.h"

#include <algorithm>
#include <string>

namespace tuxblox {

namespace {

bool sameSession(const Session& a, const Session& b) {
    return a.pid == b.pid && a.startedAt == b.startedAt;
}

void appendTarget(std::vector<Session>& into, const std::string& procRoot,
                  const std::vector<int>& pids, LaunchTarget target) {
    for (int pid : pids) {
        Session session;
        session.target = target;
        session.pid = pid;
        session.startedAt = processStartTime(procRoot, pid);
        session.residentBytes = processResidentBytes(procRoot, pid);
        session.running = true;
        into.push_back(session);
    }
}

} // namespace

std::vector<Session> liveSessionsIn(const std::string& procRoot, const std::string& prefixDir) {
    const PrefixSessions census = prefixSessionsIn(procRoot, prefixDir);
    std::vector<Session> sessions;
    appendTarget(sessions, procRoot, census.playerPids, LaunchTarget::Player);
    appendTarget(sessions, procRoot, census.studioPids, LaunchTarget::Studio);
    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) {
        if (a.startedAt != b.startedAt) return a.startedAt < b.startedAt;
        return a.pid < b.pid;
    });
    return sessions;
}

std::string describeExit(bool stopRequested, std::optional<int> markerExitCode) {
    // A stop produces a non-zero code of its own, so what the user did outranks what the code says.
    if (stopRequested) return "stopped by you";
    if (!markerExitCode.has_value()) return "closed normally";

    const int code = *markerExitCode;
    if (code == 0) return "closed normally";
    const char* pTitle = exitCodeTitle(code);
    if (pTitle != nullptr) return std::string("crashed (") + pTitle + ", " + std::to_string(code) + ")";
    return "crashed (code " + std::to_string(code) + ")";
}

std::vector<Session> mergeSessions(const std::vector<Session>& previous,
                                   const std::vector<Session>& live) {
    std::vector<Session> running;
    std::vector<Session> ended;

    for (const Session& fresh : live) {
        Session carried = fresh;
        for (const Session& old : previous) {
            if (!sameSession(old, fresh) || !old.running) continue;
            // What was learned about this session -- which log files are its -- is not rediscovered every second.
            carried.capturedLogPath = old.capturedLogPath;
            carried.robloxLogPath = old.robloxLogPath;
            break;
        }
        running.push_back(carried);
    }

    const std::time_t now = std::time(nullptr);
    for (const Session& old : previous) {
        const bool stillLive = std::any_of(live.begin(), live.end(), [&old](const Session& fresh) {
            return sameSession(old, fresh);
        });
        if (stillLive) continue;

        Session finished = old;
        // Only the moment it first went missing is recorded. The census is a second behind, so a session is absent for many passes after it ends and its row must not be rewritten on each one.
        if (finished.running) {
            finished.running = false;
            finished.endedAt = now;
        }
        ended.push_back(finished);
    }

    std::sort(ended.begin(), ended.end(), [](const Session& a, const Session& b) {
        return a.endedAt > b.endedAt;
    });
    running.insert(running.end(), ended.begin(), ended.end());
    return running;
}

} // namespace tuxblox
