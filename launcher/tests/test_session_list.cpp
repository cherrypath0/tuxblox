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

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

const long kBootTime = 1700000000;

void writeNulJoined(const fs::path& path, const std::vector<std::string>& parts) {
    std::ofstream file(path, std::ios::binary);
    for (const auto& part : parts) {
        file.write(part.data(), static_cast<std::streamsize>(part.size()));
        file.put('\0');
    }
}

// A Roblox process in the drive, with the age and memory the page reads off it.
void makeClient(const fs::path& procRoot, int pid, const std::string& image,
                const std::string& prefix, long ageSeconds, unsigned long long residentPages) {
    const fs::path dir = procRoot / std::to_string(pid);
    fs::create_directories(dir);
    writeNulJoined(dir / "cmdline", {"C:\\v\\" + image});
    writeNulJoined(dir / "environ", {"PATH=/usr/bin", "WINEPREFIX=" + prefix});

    const long ticks = sysconf(_SC_CLK_TCK);
    std::ofstream stat(dir / "stat");
    stat << pid << " (" << image << ") S";
    for (int field = 4; field <= 21; ++field) stat << " 0";
    stat << " " << (ageSeconds * ticks);
    for (int field = 23; field <= 52; ++field) stat << " 0";
    stat << "\n";

    std::ofstream(dir / "statm") << "99999 " << residentPages << " 0 0 0 0 0\n";
}

Session running(LaunchTarget target, int pid, std::time_t startedAt) {
    Session session;
    session.target = target;
    session.pid = pid;
    session.startedAt = startedAt;
    session.running = true;
    return session;
}

void test_lists_every_live_session_whatever_started_it(const fs::path& procRoot,
                                                        const std::string& prefix) {
    const std::vector<Session> live = liveSessionsIn(procRoot.string(), prefix);
    assert(live.size() == 3);
    // A roblox:// link starts a session with no window; opening the launcher afterwards must still list it.
    bool sawPlayer = false;
    for (const Session& session : live) {
        if (session.target == LaunchTarget::Player) sawPlayer = true;
        assert(session.running);
        assert(session.startedAt > 0);
        assert(session.residentBytes > 0);
    }
    assert(sawPlayer);
}

void test_sorted_oldest_first(const fs::path& procRoot, const std::string& prefix) {
    const std::vector<Session> live = liveSessionsIn(procRoot.string(), prefix);
    for (size_t i = 1; i < live.size(); ++i) assert(live[i - 1].startedAt <= live[i].startedAt);
}

void test_carries_a_running_session_forward_keeping_its_log_paths() {
    Session before = running(LaunchTarget::Studio, 200, 1000);
    before.capturedLogPath = "/logs/Studio-x.log";
    before.robloxLogPath = "/drive/roblox_studio.log";

    const std::vector<Session> merged = mergeSessions({before}, {running(LaunchTarget::Studio, 200, 1000)});
    assert(merged.size() == 1);
    assert(merged[0].running);
    assert(merged[0].capturedLogPath == "/logs/Studio-x.log");
    assert(merged[0].robloxLogPath == "/drive/roblox_studio.log");
}

void test_a_vanished_session_becomes_ended_with_a_time() {
    const std::vector<Session> merged = mergeSessions({running(LaunchTarget::Studio, 200, 1000)}, {});
    assert(merged.size() == 1);
    assert(!merged[0].running);
    assert(merged[0].endedAt > 0);
}

// The census is a second old, so a session is absent for many ticks after it ends. Its row must
// not be rebuilt with a fresh ended time on each of them.
void test_an_ended_session_stays_ended_and_does_not_flicker() {
    const std::vector<Session> once = mergeSessions({running(LaunchTarget::Studio, 200, 1000)}, {});
    const std::vector<Session> twice = mergeSessions(once, {});
    const std::vector<Session> thrice = mergeSessions(twice, {});
    assert(thrice.size() == 1);
    assert(!thrice[0].running);
    assert(thrice[0].endedAt == once[0].endedAt);
    assert(thrice[0].endedReason == once[0].endedReason);
}

// Linux reuses pids, so identity is the pid and when it started, never the pid alone.
void test_a_reused_pid_is_a_new_session() {
    const std::vector<Session> merged =
        mergeSessions({running(LaunchTarget::Studio, 200, 1000)},
                      {running(LaunchTarget::Studio, 200, 5000)});
    assert(merged.size() == 2);
    size_t live = 0;
    for (const Session& session : merged) {
        if (session.running) ++live;
    }
    assert(live == 1);
}

void test_running_before_ended_in_the_result() {
    const std::vector<Session> merged =
        mergeSessions({running(LaunchTarget::Studio, 200, 1000)},
                      {running(LaunchTarget::Studio, 201, 2000)});
    assert(merged.size() == 2);
    assert(merged[0].running);
    assert(!merged[1].running);
}

// Stopping a session yourself is what was meant to happen, so the row says nothing about it.
void test_describe_exit_says_nothing_about_a_stop() {
    assert(describeExit(true, std::nullopt).empty());
}

void test_describe_exit_says_nothing_about_an_ordinary_close() {
    assert(describeExit(false, std::nullopt).empty());
    assert(describeExit(false, 0).empty());
}

void test_describe_exit_reports_a_crash() {
    assert(describeExit(false, 139) == "crashed");
    assert(describeExit(false, 4242) == "crashed");
}

// Stopping a session produces a non-zero code of its own; that is not a crash.
void test_describe_exit_stop_wins_over_a_code() {
    assert(describeExit(true, 143).empty());
}

void test_nothing_new_means_no_arrival() {
    const std::vector<Session> both = {running(LaunchTarget::Studio, 10, 100),
                                       running(LaunchTarget::Player, 11, 200)};
    assert(arrivedSession(both, both, LaunchTarget::Studio) == 0);
}

void test_the_one_that_turned_up_is_the_arrival() {
    const std::vector<Session> before = {running(LaunchTarget::Studio, 10, 100)};
    const std::vector<Session> after = {running(LaunchTarget::Studio, 10, 100),
                                        running(LaunchTarget::Studio, 12, 300)};
    assert(arrivedSession(before, after, LaunchTarget::Studio) == 12);
}

void test_the_app_that_was_asked_for_wins() {
    // A Player session of somebody else's starting in the same pass must not steal a Studio launch's jump.
    const std::vector<Session> before = {running(LaunchTarget::Studio, 10, 100)};
    const std::vector<Session> after = {running(LaunchTarget::Studio, 10, 100),
                                        running(LaunchTarget::Studio, 12, 300),
                                        running(LaunchTarget::Player, 13, 400)};
    assert(arrivedSession(before, after, LaunchTarget::Studio) == 12);
    assert(arrivedSession(before, after, LaunchTarget::Player) == 13);
}

void test_a_session_of_another_app_still_counts_when_nothing_matches() {
    const std::vector<Session> before = {running(LaunchTarget::Studio, 10, 100)};
    const std::vector<Session> after = {running(LaunchTarget::Studio, 10, 100),
                                        running(LaunchTarget::Player, 13, 400)};
    assert(arrivedSession(before, after, LaunchTarget::Studio) == 13);
}

void test_the_newest_of_several_arrivals_wins() {
    const std::vector<Session> before = {};
    const std::vector<Session> after = {running(LaunchTarget::Studio, 10, 100),
                                        running(LaunchTarget::Studio, 12, 300)};
    assert(arrivedSession(before, after, LaunchTarget::Studio) == 12);
}

void test_a_reused_pid_counts_as_an_arrival() {
    // Same number, different session: the one being read is gone and the new one is what a launch produced.
    const std::vector<Session> before = {running(LaunchTarget::Studio, 10, 100)};
    const std::vector<Session> after = {running(LaunchTarget::Studio, 10, 500)};
    assert(arrivedSession(before, after, LaunchTarget::Studio) == 10);
}

} // namespace

int main() {
    const fs::path tmp = fs::temp_directory_path() / "tuxblox_session_list_test";
    fs::remove_all(tmp);
    const fs::path procRoot = tmp / "proc";
    fs::create_directories(procRoot);
    std::ofstream(procRoot / "stat") << "cpu 1 2 3\nbtime " << kBootTime << "\nprocesses 99\n";

    const std::string prefix = (tmp / "runtime" / "pfx").string();
    const std::string elsewhere = (tmp / "other" / "pfx").string();
    makeClient(procRoot, 200, "RobloxStudioBeta.exe", prefix, 1800, 4096);
    makeClient(procRoot, 201, "RobloxStudioBeta.exe", prefix, 600, 2048);
    makeClient(procRoot, 202, "RobloxPlayerBeta.exe", prefix, 3600, 8192);
    // Another drive's session is not this drive's business.
    makeClient(procRoot, 203, "RobloxStudioBeta.exe", elsewhere, 900, 1024);

    test_lists_every_live_session_whatever_started_it(procRoot, prefix);
    test_sorted_oldest_first(procRoot, prefix);
    test_carries_a_running_session_forward_keeping_its_log_paths();
    test_a_vanished_session_becomes_ended_with_a_time();
    test_an_ended_session_stays_ended_and_does_not_flicker();
    test_a_reused_pid_is_a_new_session();
    test_running_before_ended_in_the_result();
    test_describe_exit_says_nothing_about_a_stop();
    test_describe_exit_says_nothing_about_an_ordinary_close();
    test_describe_exit_reports_a_crash();
    test_describe_exit_stop_wins_over_a_code();
    test_nothing_new_means_no_arrival();
    test_the_one_that_turned_up_is_the_arrival();
    test_the_app_that_was_asked_for_wins();
    test_a_session_of_another_app_still_counts_when_nothing_matches();
    test_the_newest_of_several_arrivals_wins();
    test_a_reused_pid_counts_as_an_arrival();

    fs::remove_all(tmp);
    std::printf("session_list: all tests passed\n");
    return 0;
}
