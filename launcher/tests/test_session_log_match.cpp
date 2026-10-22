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

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

// The shapes Roblox actually writes, taken from a live install.
const char kClientLog[] = "0.742.0.7421053_20261022T172658Z_Player_4D8AA_last.log";
const char kCrashLog[] = "0.742.0.7421053_20261022T172630Z_Player_B78E5_CrashHandler_last.log";
const char kStudioLog[] = "0.742.0.7421053_20261022T174708Z_Studio_B467A_last.log";

Session startedAt(LaunchTarget target, int pid, std::time_t when) {
    Session session;
    session.target = target;
    session.pid = pid;
    session.startedAt = when;
    session.running = true;
    return session;
}

LogCandidate candidate(const std::string& path, LaunchTarget target, std::time_t when) {
    LogCandidate entry;
    entry.path = path;
    entry.target = target;
    entry.startedAt = when;
    return entry;
}

// Roblox's crash handler writes its own log carrying the same _Player_ or _Studio_ marker the
// client's does, so a matcher that keys only on that marker will show it as the session's log.
void test_crash_handler_logs_are_rejected() {
    assert(isCrashHandlerLog(kCrashLog));
    assert(isCrashHandlerLog("0.1_20261022T000000Z_Studio_AAAAA_CrashHandler_last.log"));
}

void test_a_normal_client_log_is_not_rejected() {
    assert(!isCrashHandlerLog(kClientLog));
    assert(!isCrashHandlerLog(kStudioLog));
}

// The log names its own start, which does not move as the file is written to, unlike its mtime.
void test_reads_the_start_time_out_of_the_name() {
    const std::time_t when = robloxLogStartTime(kClientLog);
    assert(when > 0);
    // A later log in the same run must read as later.
    assert(robloxLogStartTime(kStudioLog) > when);
}

void test_an_unparseable_name_has_no_start_time() {
    assert(robloxLogStartTime("not-a-roblox-log.txt") == 0);
    assert(robloxLogStartTime("") == 0);
}

void test_matches_one_log_to_one_session() {
    const std::vector<LogCandidate> candidates{candidate(kStudioLog, LaunchTarget::Studio, 2000)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000)};
    assert(matchSessionLogs(candidates, sessions) == std::vector<std::string>{kStudioLog});
}

// Newest-wins hands both sessions the same file; this is the case the page exists for.
void test_two_studio_sessions_get_different_logs() {
    const std::vector<LogCandidate> candidates{
        candidate("/logs/first.log", LaunchTarget::Studio, 1100),
        candidate("/logs/second.log", LaunchTarget::Studio, 2100)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000),
                                        startedAt(LaunchTarget::Studio, 201, 2000)};
    const std::vector<std::string> matched = matchSessionLogs(candidates, sessions);
    assert(matched.size() == 2);
    assert(matched[0] == "/logs/first.log");
    assert(matched[1] == "/logs/second.log");
    assert(matched[0] != matched[1]);
}

void test_a_log_older_than_the_session_is_never_matched() {
    const std::vector<LogCandidate> candidates{candidate("/logs/old.log", LaunchTarget::Studio, 500)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000)};
    assert(matchSessionLogs(candidates, sessions) == std::vector<std::string>{""});
}

// A launch that dies before Roblox writes anything. The pane turns the empty path into a message;
// a blank pane would be indistinguishable from a working session with a quiet log.
void test_a_session_with_no_candidate_gets_an_empty_path() {
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000)};
    assert(matchSessionLogs({}, sessions) == std::vector<std::string>{""});
}

// The start time comes from /proc, not from having watched the launch, so a session started by a
// roblox:// link before the launcher opened matches exactly like any other.
void test_a_session_started_before_the_launcher_opened_still_matches() {
    const std::vector<LogCandidate> candidates{candidate("/logs/early.log", LaunchTarget::Player, 110)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Player, 200, 100)};
    assert(matchSessionLogs(candidates, sessions) == std::vector<std::string>{"/logs/early.log"});
}

void test_the_target_must_agree() {
    const std::vector<LogCandidate> candidates{candidate(kStudioLog, LaunchTarget::Studio, 2000)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Player, 200, 1000)};
    assert(matchSessionLogs(candidates, sessions) == std::vector<std::string>{""});
}

void test_a_claimed_file_is_not_offered_twice() {
    const std::vector<LogCandidate> candidates{candidate("/logs/only.log", LaunchTarget::Studio, 1100)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000),
                                        startedAt(LaunchTarget::Studio, 201, 1050)};
    const std::vector<std::string> matched = matchSessionLogs(candidates, sessions);
    assert(matched.size() == 2);
    assert(matched[0] == "/logs/only.log");
    assert(matched[1].empty());
}

// TuxBlox's own capture is created just BEFORE the session it wraps, so the bound runs the other
// way from Roblox's log. It carries the exit-code marker, which is how an ended row says why.
void test_the_capture_is_matched_backwards_in_time() {
    const std::vector<LogCandidate> candidates{
        candidate("/logs/Studio-20261022T183000Z-11.log", LaunchTarget::Studio, 900),
        candidate("/logs/Studio-20261022T184000Z-12.log", LaunchTarget::Studio, 1500)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000)};
    // The one just before the session, not the one after it.
    assert(matchCapturedLogs(candidates, sessions) == std::vector<std::string>{"/logs/Studio-20261022T183000Z-11.log"});
}

void test_the_capture_takes_the_closest_one_before() {
    const std::vector<LogCandidate> candidates{
        candidate("/logs/old.log", LaunchTarget::Studio, 100),
        candidate("/logs/near.log", LaunchTarget::Studio, 990)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000)};
    assert(matchCapturedLogs(candidates, sessions) == std::vector<std::string>{"/logs/near.log"});
}

void test_two_sessions_do_not_share_a_capture() {
    const std::vector<LogCandidate> candidates{
        candidate("/logs/first.log", LaunchTarget::Studio, 900),
        candidate("/logs/second.log", LaunchTarget::Studio, 1900)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 1000),
                                        startedAt(LaunchTarget::Studio, 201, 2000)};
    const std::vector<std::string> matched = matchCapturedLogs(candidates, sessions);
    assert(matched[0] == "/logs/first.log");
    assert(matched[1] == "/logs/second.log");
}

void test_a_capture_far_before_the_session_is_not_matched() {
    const std::vector<LogCandidate> candidates{candidate("/logs/ancient.log", LaunchTarget::Studio, 0)};
    const std::vector<Session> sessions{startedAt(LaunchTarget::Studio, 200, 100000)};
    assert(matchCapturedLogs(candidates, sessions) == std::vector<std::string>{""});
}

void test_an_ended_session_still_keeps_its_log() {
    Session ended = startedAt(LaunchTarget::Studio, 200, 1000);
    ended.running = false;
    ended.endedAt = 3000;
    const std::vector<LogCandidate> candidates{candidate(kStudioLog, LaunchTarget::Studio, 1100)};
    assert(matchSessionLogs(candidates, {ended}) == std::vector<std::string>{kStudioLog});
}

} // namespace

int main() {
    test_crash_handler_logs_are_rejected();
    test_a_normal_client_log_is_not_rejected();
    test_reads_the_start_time_out_of_the_name();
    test_an_unparseable_name_has_no_start_time();
    test_matches_one_log_to_one_session();
    test_two_studio_sessions_get_different_logs();
    test_a_log_older_than_the_session_is_never_matched();
    test_a_session_with_no_candidate_gets_an_empty_path();
    test_a_session_started_before_the_launcher_opened_still_matches();
    test_the_target_must_agree();
    test_a_claimed_file_is_not_offered_twice();
    test_the_capture_is_matched_backwards_in_time();
    test_the_capture_takes_the_closest_one_before();
    test_two_sessions_do_not_share_a_capture();
    test_a_capture_far_before_the_session_is_not_matched();
    test_an_ended_session_still_keeps_its_log();
    std::printf("session_log_match: all tests passed\n");
    return 0;
}
