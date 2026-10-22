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

#include "session_log_match.h"

#include <cstdlib>
#include <cstring>

namespace tuxblox {

namespace {

bool allDigits(const std::string& text, size_t from, size_t count) {
    if (from + count > text.size()) return false;
    for (size_t i = from; i < from + count; ++i) {
        if (text[i] < '0' || text[i] > '9') return false;
    }
    return true;
}

int number(const std::string& text, size_t from, size_t count) {
    return std::atoi(text.substr(from, count).c_str());
}

} // namespace

bool isCrashHandlerLog(const std::string& filename) {
    return filename.find("_CrashHandler_") != std::string::npos;
}

std::time_t robloxLogStartTime(const std::string& filename) {
    // The name carries "<version>_YYYYMMDDTHHMMSSZ_<app>_...", so the stamp is found by its shape rather than by position.
    for (size_t at = 0; at + 16 <= filename.size(); ++at) {
        if (!allDigits(filename, at, 8)) continue;
        if (filename[at + 8] != 'T') continue;
        if (!allDigits(filename, at + 9, 6)) continue;
        if (filename[at + 15] != 'Z') continue;

        std::tm when{};
        when.tm_year = number(filename, at, 4) - 1900;
        when.tm_mon = number(filename, at + 4, 2) - 1;
        when.tm_mday = number(filename, at + 6, 2);
        when.tm_hour = number(filename, at + 9, 2);
        when.tm_min = number(filename, at + 11, 2);
        when.tm_sec = number(filename, at + 13, 2);
        // The stamp is UTC, so it is read as UTC rather than through the machine's own time zone.
        return timegm(&when);
    }
    return 0;
}

std::vector<std::string> matchSessionLogs(const std::vector<LogCandidate>& candidates,
                                          const std::vector<Session>& sessions) {
    std::vector<std::string> matched(sessions.size());
    std::vector<bool> claimed(candidates.size(), false);

    for (size_t s = 0; s < sessions.size(); ++s) {
        const Session& session = sessions[s];
        size_t best = candidates.size();
        for (size_t c = 0; c < candidates.size(); ++c) {
            if (claimed[c]) continue;
            const LogCandidate& candidate = candidates[c];
            if (candidate.target != session.target) continue;
            if (isCrashHandlerLog(candidate.path)) continue;
            // Roblox writes its log after its process exists, so a log older than the session was written by a different one.
            if (candidate.startedAt < session.startedAt) continue;
            if (best == candidates.size() || candidate.startedAt < candidates[best].startedAt) best = c;
        }
        if (best == candidates.size()) continue;
        claimed[best] = true;
        matched[s] = candidates[best].path;
    }
    return matched;
}

std::vector<std::string> matchCapturedLogs(const std::vector<LogCandidate>& candidates,
                                           const std::vector<Session>& sessions) {
    std::vector<std::string> matched(sessions.size());
    std::vector<bool> claimed(candidates.size(), false);

    for (size_t s = 0; s < sessions.size(); ++s) {
        const Session& session = sessions[s];
        size_t best = candidates.size();
        for (size_t c = 0; c < candidates.size(); ++c) {
            if (claimed[c]) continue;
            const LogCandidate& candidate = candidates[c];
            if (candidate.target != session.target) continue;
            if (candidate.startedAt > session.startedAt) continue;
            if (session.startedAt - candidate.startedAt > kCaptureLeadSeconds) continue;
            // The closest one before the session, since an older one belongs to an earlier run.
            if (best == candidates.size() || candidate.startedAt > candidates[best].startedAt) best = c;
        }
        if (best == candidates.size()) continue;
        claimed[best] = true;
        matched[s] = candidates[best].path;
    }
    return matched;
}

} // namespace tuxblox
