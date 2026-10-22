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

#include "ui_adw/sessions_page_state.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

const std::time_t kNow = 1700010000;

Session alive(LaunchTarget target, int pid, long ageSeconds, unsigned long long bytes) {
    Session session;
    session.target = target;
    session.pid = pid;
    session.startedAt = kNow - ageSeconds;
    session.residentBytes = bytes;
    session.running = true;
    return session;
}

Session finished(LaunchTarget target, int pid, long endedAgo, const std::string& reason) {
    Session session;
    session.target = target;
    session.pid = pid;
    session.startedAt = kNow - 3600;
    session.running = false;
    session.endedAt = kNow - endedAgo;
    session.endedReason = reason;
    return session;
}

void test_a_running_row_reads_uptime_pid_and_memory() {
    const SessionsView view = sessionsView({alive(LaunchTarget::Studio, 24881, 1080, 1513778872ULL)}, kNow);
    assert(view.running.size() == 1);
    const SessionRow& row = view.running[0];
    assert(row.name == "Roblox Studio");
    assert(row.running);
    assert(row.pid == 24881);
    assert(row.meta.find("18m") != std::string::npos);
    assert(row.meta.find("24881") != std::string::npos);
    assert(row.meta.find("1.41 GB") != std::string::npos);
}

void test_an_ended_row_reads_how_long_ago_and_why() {
    const SessionsView view = sessionsView({finished(LaunchTarget::Player, 11, 360, "closed normally")}, kNow);
    assert(view.ended.size() == 1);
    const SessionRow& row = view.ended[0];
    assert(row.name == "Roblox Player");
    assert(!row.running);
    assert(row.meta.find("6m") != std::string::npos);
    assert(row.meta.find("closed normally") != std::string::npos);
}

void test_a_crashed_row_is_marked_crashed() {
    const SessionsView view = sessionsView(
        {finished(LaunchTarget::Player, 11, 60, "crashed (Segmentation Fault, 139)"),
         finished(LaunchTarget::Studio, 12, 60, "closed normally")},
        kNow);
    assert(view.ended.size() == 2);
    assert(view.ended[0].crashed);
    assert(!view.ended[1].crashed);
}

void test_running_and_ended_are_separated() {
    const SessionsView view = sessionsView(
        {alive(LaunchTarget::Studio, 1, 60, 1024), finished(LaunchTarget::Player, 2, 60, "closed normally")},
        kNow);
    assert(view.running.size() == 1);
    assert(view.ended.size() == 1);
    assert(view.emptyMessage.empty());
}

void test_the_empty_message_appears_only_with_no_sessions_at_all() {
    assert(!sessionsView({}, kNow).emptyMessage.empty());
    assert(sessionsView({alive(LaunchTarget::Studio, 1, 60, 1024)}, kNow).emptyMessage.empty());
    assert(sessionsView({finished(LaunchTarget::Studio, 1, 60, "closed normally")}, kNow).emptyMessage.empty());
}

// Nothing in the window may require knowing what any of this is made of.
void test_no_row_mentions_a_layer_process_or_the_drive() {
    const SessionsView view = sessionsView(
        {alive(LaunchTarget::Studio, 1, 60, 1024), finished(LaunchTarget::Player, 2, 60, "closed normally")},
        kNow);
    // "layer" on its own is not a usable guard -- "Roblox Player" contains it.
    const char* const forbidden[] = {"wine", "Wine", "prefix", "proton", "Proton", "pfx",
                                     "compatibility layer", "wineserver", "/proc", "process"};
    for (const std::vector<SessionRow>* group : {&view.running, &view.ended}) {
        for (const SessionRow& row : *group) {
            for (const char* word : forbidden) {
                assert(row.name.find(word) == std::string::npos);
                assert(row.meta.find(word) == std::string::npos);
            }
        }
    }
    assert(sessionsView({}, kNow).emptyMessage.find("prefix") == std::string::npos);
}

void test_filter_matches_the_app_name() {
    const SessionsView view = sessionsView(
        {alive(LaunchTarget::Studio, 1, 60, 1024), alive(LaunchTarget::Player, 2, 60, 1024)}, kNow);
    const SessionsView onlyStudio = filterSessions(view, "studio");
    assert(onlyStudio.running.size() == 1);
    assert(onlyStudio.running[0].name == "Roblox Studio");
}

void test_filter_matches_the_pid() {
    const SessionsView view = sessionsView(
        {alive(LaunchTarget::Studio, 24881, 60, 1024), alive(LaunchTarget::Studio, 25402, 60, 1024)}, kNow);
    const SessionsView one = filterSessions(view, "25402");
    assert(one.running.size() == 1);
    assert(one.running[0].pid == 25402);
}

void test_filter_with_an_empty_query_keeps_everything() {
    const SessionsView view = sessionsView(
        {alive(LaunchTarget::Studio, 1, 60, 1024), finished(LaunchTarget::Player, 2, 60, "closed normally")},
        kNow);
    const SessionsView all = filterSessions(view, "");
    assert(all.running.size() == 1);
    assert(all.ended.size() == 1);
}

void test_filter_that_matches_nothing_says_so() {
    const SessionsView view = sessionsView({alive(LaunchTarget::Studio, 1, 60, 1024)}, kNow);
    const SessionsView none = filterSessions(view, "zzzz");
    assert(none.running.empty());
    assert(none.ended.empty());
    assert(!none.emptyMessage.empty());
}

} // namespace

int main() {
    test_a_running_row_reads_uptime_pid_and_memory();
    test_an_ended_row_reads_how_long_ago_and_why();
    test_a_crashed_row_is_marked_crashed();
    test_running_and_ended_are_separated();
    test_the_empty_message_appears_only_with_no_sessions_at_all();
    test_no_row_mentions_a_layer_process_or_the_drive();
    test_filter_matches_the_app_name();
    test_filter_matches_the_pid();
    test_filter_with_an_empty_query_keeps_everything();
    test_filter_that_matches_nothing_says_so();
    std::printf("sessions_page_state: all tests passed\n");
    return 0;
}
