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
#include "session_facts.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>
#include <unistd.h>

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

std::vector<Session> macSessionsIn(const std::string& procRoot, const std::string& installDir) {
    std::vector<Session> sessions;
    const std::string wantedPrefix =
        (std::filesystem::path(installDir) / "mac-client" / "prefix").string();
    const pid_t self = ::getpid();
    std::error_code ec;
    std::filesystem::directory_iterator it(procRoot, ec), end;
    for (; it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_directory(ec)) continue;
        const std::string name = it->path().filename().string();
        if (name.empty() || !std::isdigit(static_cast<unsigned char>(name[0]))) continue;
        const int pid = std::atoi(name.c_str());
        if (pid <= 0 || pid == self) continue;

        std::ifstream environ(it->path() / "environ", std::ios::binary);
        std::istreambuf_iterator<char> first(environ), last;
        const std::string blob(first, last);
        if (envValueFromEnviron(blob, "TUXBLOX_MAC_CLIENT") != "1") continue;
        const std::string prefix = envValueFromEnviron(blob, "ROSEMARY_PREFIX");
        if (!prefix.empty() && prefix != wantedPrefix) continue;

        // Every guest Rosemary starts inherits the marker above: the crash
        // handler, the menu bar app and the prefix services would otherwise
        // each show up as a separate Roblox session. Only the process running
        // the main client executable is one.
        std::ifstream cmdlineFile(it->path() / "cmdline", std::ios::binary);
        std::istreambuf_iterator<char> cmdFirst(cmdlineFile), cmdLast;
        const std::string cmdline(cmdFirst, cmdLast);
        const std::string clientRoot =
            (std::filesystem::path(installDir) / "mac-client" / "player").string();
        if (cmdline.find(clientRoot) == std::string::npos ||
            cmdline.find("RobloxPlayer.app/Contents/MacOS/RobloxPlayer") == std::string::npos)
            continue;

        Session session;
        session.target = LaunchTarget::Player;
        session.pid = pid;
        session.startedAt = processStartTime(procRoot, pid);
        session.residentBytes = processResidentBytes(procRoot, pid);
        session.running = true;
        sessions.push_back(session);
    }
    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) {
        if (a.startedAt != b.startedAt) return a.startedAt < b.startedAt;
        return a.pid < b.pid;
    });
    return sessions;
}

std::vector<Session> allLiveSessionsIn(const std::string& procRoot,
                                       const std::string& installDir) {
    std::vector<Session> sessions =
        liveSessionsIn(procRoot, installDir + "/runtime/pfx");
    std::vector<Session> mac = macSessionsIn(procRoot, installDir);
    sessions.insert(sessions.end(), mac.begin(), mac.end());
    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) {
        if (a.startedAt != b.startedAt) return a.startedAt < b.startedAt;
        return a.pid < b.pid;
    });
    return sessions;
}

std::string describeExit(bool stopRequested, std::optional<int> markerExitCode) {
    // Closing Roblox and stopping it yourself are both what was meant to happen, so neither is worth a line; only a crash is.
    if (stopRequested) return "";
    if (!markerExitCode.has_value() || *markerExitCode == 0) return "";
    return "crashed";
}

int arrivedSession(const std::vector<Session>& previous, const std::vector<Session>& live,
                   LaunchTarget wanted) {
    int anyNew = 0;
    int matching = 0;
    for (const Session& fresh : live) {
        const bool knownAlready = std::any_of(previous.begin(), previous.end(),
                                              [&fresh](const Session& old) { return sameSession(old, fresh); });
        if (knownAlready) continue;
        // The list runs oldest first, so a later one replaces an earlier: the newest arrival wins.
        anyNew = fresh.pid;
        if (fresh.target == wanted) matching = fresh.pid;
    }
    return matching != 0 ? matching : anyNew;
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
