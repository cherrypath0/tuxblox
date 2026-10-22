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

#include "session_facts.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

const long kBootTime = 1700000000;

// /proc/<pid>/stat: field 1 is the pid, field 2 the executable name in brackets, field 22 the
// start time in clock ticks. Fields 3 to 21 are filled with a placeholder.
void writeStat(const fs::path& procRoot, int pid, const std::string& comm, long long startTicks) {
    fs::create_directories(procRoot / std::to_string(pid));
    std::ofstream out(procRoot / std::to_string(pid) / "stat");
    out << pid << " (" << comm << ") S";
    for (int field = 4; field <= 21; ++field) out << " 0";
    out << " " << startTicks;
    for (int field = 23; field <= 52; ++field) out << " 0";
    out << "\n";
}

void writeStatm(const fs::path& procRoot, int pid, unsigned long long residentPages) {
    fs::create_directories(procRoot / std::to_string(pid));
    std::ofstream(procRoot / std::to_string(pid) / "statm")
        << "99999 " << residentPages << " 0 0 0 0 0\n";
}

void test_reads_boot_time_from_stat(const fs::path& procRoot) {
    assert(bootTimeSeconds(procRoot.string()) == kBootTime);
}

void test_boot_time_is_zero_without_a_proc(const fs::path& tmp) {
    assert(bootTimeSeconds((tmp / "nope").string()) == 0);
}

void test_start_time_is_boot_plus_field_22(const fs::path& procRoot) {
    const long ticks = sysconf(_SC_CLK_TCK);
    writeStat(procRoot, 200, "RobloxStudioBeta.exe", 3600 * ticks);
    assert(processStartTime(procRoot.string(), 200) == kBootTime + 3600);
}

// The executable name is field 2 and may hold spaces and brackets of its own, so splitting the
// line on whitespace from the left reads the wrong field and the session looks decades old.
void test_start_time_survives_a_comm_with_spaces(const fs::path& procRoot) {
    const long ticks = sysconf(_SC_CLK_TCK);
    writeStat(procRoot, 201, "Roblox Player Beta (x64)", 1800 * ticks);
    assert(processStartTime(procRoot.string(), 201) == kBootTime + 1800);
}

void test_start_time_is_zero_for_a_missing_pid(const fs::path& procRoot) {
    assert(processStartTime(procRoot.string(), 999999) == 0);
}

void test_resident_bytes_from_statm(const fs::path& procRoot) {
    const unsigned long long pageSize = static_cast<unsigned long long>(sysconf(_SC_PAGESIZE));
    writeStatm(procRoot, 200, 4096);
    assert(processResidentBytes(procRoot.string(), 200) == 4096ULL * pageSize);
}

void test_resident_bytes_is_zero_for_a_missing_pid(const fs::path& procRoot) {
    assert(processResidentBytes(procRoot.string(), 999999) == 0);
}

void test_uptime_formats() {
    assert(formatUptime(41) == "41s");
    assert(formatUptime(1080) == "18m");
    assert(formatUptime(3840) == "1h 04m");
    assert(formatUptime(183600) == "2d 03h");
}

void test_uptime_never_empty_for_zero_or_negative() {
    assert(!formatUptime(0).empty());
    assert(!formatUptime(-5).empty());
}

void test_byte_formats() {
    assert(formatBytes(1513778872ULL) == "1.41 GB");
    assert(formatBytes(851443712ULL) == "812 MB");
    assert(formatBytes(0) == "0 MB");
}

} // namespace

int main() {
    const fs::path tmp = fs::temp_directory_path() / "tuxblox_session_facts_test";
    fs::remove_all(tmp);
    const fs::path procRoot = tmp / "proc";
    fs::create_directories(procRoot);
    std::ofstream(procRoot / "stat") << "cpu 1 2 3\nbtime " << kBootTime << "\nprocesses 99\n";

    test_reads_boot_time_from_stat(procRoot);
    test_boot_time_is_zero_without_a_proc(tmp);
    test_start_time_is_boot_plus_field_22(procRoot);
    test_start_time_survives_a_comm_with_spaces(procRoot);
    test_start_time_is_zero_for_a_missing_pid(procRoot);
    test_resident_bytes_from_statm(procRoot);
    test_resident_bytes_is_zero_for_a_missing_pid(procRoot);
    test_uptime_formats();
    test_uptime_never_empty_for_zero_or_negative();
    test_byte_formats();

    fs::remove_all(tmp);
    std::printf("session_facts: all tests passed\n");
    return 0;
}
