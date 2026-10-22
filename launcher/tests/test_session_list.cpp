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

void test_describe_exit_stopped_by_you() {
    assert(describeExit(true, std::nullopt) == "stopped by you");
}

void test_describe_exit_closed_normally() {
    assert(describeExit(false, std::nullopt) == "closed normally");
}

void test_describe_exit_names_the_code() {
    const std::string named = describeExit(false, 139);
    assert(named.find("139") != std::string::npos);
    assert(named.find("Segmentation Fault") != std::string::npos);
    // A code with no entry in the table shows the bare number rather than an invented description.
    const std::string bare = describeExit(false, 4242);
    assert(bare.find("4242") != std::string::npos);
}

// Stopping a session produces a non-zero code of its own; that is not a crash.
void test_describe_exit_stop_wins_over_a_code() {
    assert(describeExit(true, 143) == "stopped by you");
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
    test_describe_exit_stopped_by_you();
    test_describe_exit_closed_normally();
    test_describe_exit_names_the_code();
    test_describe_exit_stop_wins_over_a_code();

    fs::remove_all(tmp);
    std::printf("session_list: all tests passed\n");
    return 0;
}
