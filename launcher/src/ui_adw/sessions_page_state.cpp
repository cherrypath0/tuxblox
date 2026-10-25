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

#include "sessions_page_state.h"

#include "session_facts.h"

#include <algorithm>
#include <cctype>

namespace tuxblox {

namespace {

const char kNothingRunning[] = "Nothing is running. Launch Roblox from Home and it will appear here.";
const char kNothingMatched[] = "No sessions match what you typed.";
const char kSeparator[] = " \xC2\xB7 ";

std::string lowered(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string appName(LaunchTarget target) {
    return target == LaunchTarget::Player ? "Roblox Player" : "Roblox Studio";
}

bool matches(const SessionRow& row, const std::string& needle) {
    return lowered(row.name).find(needle) != std::string::npos ||
           lowered(row.meta).find(needle) != std::string::npos;
}

} // namespace

SessionsView sessionsView(const std::vector<Session>& sessions, std::time_t now) {
    SessionsView view;
    for (const Session& session : sessions) {
        SessionRow row;
        row.name = appName(session.target);
        row.running = session.running;
        row.pid = session.pid;

        if (session.running) {
            const long age = static_cast<long>(now - session.startedAt);
            row.meta = formatUptime(age) + kSeparator + "PID " + std::to_string(session.pid);
            if (session.residentBytes > 0) row.meta += std::string(kSeparator) + formatBytes(session.residentBytes);
            view.running.push_back(std::move(row));
            continue;
        }

        const long since = static_cast<long>(now - session.endedAt);
        row.meta = "ended " + formatUptime(since) + " ago";
        if (!session.endedReason.empty()) row.meta += std::string(kSeparator) + session.endedReason;
        // Anything that did not close cleanly reads differently, so a crash is not something to go hunting for.
        row.crashed = session.endedReason == "crashed";
        view.ended.push_back(std::move(row));
    }

    if (view.running.empty() && view.ended.empty()) view.emptyMessage = kNothingRunning;
    return view;
}

SessionsView filterSessions(const SessionsView& view, const std::string& query) {
    if (query.empty()) return view;
    const std::string needle = lowered(query);

    SessionsView kept;
    for (const SessionRow& row : view.running) {
        if (matches(row, needle)) kept.running.push_back(row);
    }
    for (const SessionRow& row : view.ended) {
        if (matches(row, needle)) kept.ended.push_back(row);
    }
    if (kept.running.empty() && kept.ended.empty()) {
        kept.emptyMessage = view.running.empty() && view.ended.empty() ? kNothingRunning : kNothingMatched;
    }
    return kept;
}

} // namespace tuxblox
