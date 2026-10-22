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

#include "session_log_reader.h"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace tuxblox;

namespace {

void append(const fs::path& path, const std::string& text) {
    std::ofstream file(path, std::ios::app | std::ios::binary);
    file << text;
}

// The newest entry is held until something proves it finished, so reading everything available
// means pumping until it stops producing.
std::vector<LogEntry> drainAll(SessionLogReader& reader) {
    std::vector<LogEntry> all;
    for (int attempt = 0; attempt < 4; ++attempt) {
        const std::vector<LogEntry> got = reader.pump();
        all.insert(all.end(), got.begin(), got.end());
    }
    return all;
}

bool anyTextContains(const std::vector<LogEntry>& entries, const std::string& needle) {
    for (const LogEntry& entry : entries) {
        if (entry.text.find(needle) != std::string::npos) return true;
    }
    return false;
}

void test_returns_nothing_before_anything_is_written(const fs::path& dir) {
    SessionLogReader reader((dir / "a-captured.log").string(), (dir / "a-roblox.log").string());
    assert(reader.pump().empty());
}

void test_returns_new_entries_as_they_arrive(const fs::path& dir) {
    const fs::path captured = dir / "b-captured.log";
    SessionLogReader reader(captured.string(), (dir / "b-roblox.log").string());
    assert(reader.pump().empty());
    append(captured, "TuxBlox: first\n");
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 1);
    assert(anyTextContains(got, "first"));
}

void test_a_second_pump_returns_only_what_is_new(const fs::path& dir) {
    const fs::path captured = dir / "c-captured.log";
    append(captured, "TuxBlox: one\n");
    SessionLogReader reader(captured.string(), (dir / "c-roblox.log").string());
    assert(drainAll(reader).size() == 1);
    assert(reader.pump().empty());
    append(captured, "TuxBlox: two\n");
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 1);
    assert(anyTextContains(got, "two"));
}

void test_reads_both_files(const fs::path& dir) {
    const fs::path captured = dir / "d-captured.log";
    const fs::path roblox = dir / "d-roblox.log";
    append(captured, "TuxBlox: from the layer\n");
    append(roblox, "2026-10-22T18:00:00.000Z,1.0,03e8,6 [FLog::Output] from roblox\n");
    SessionLogReader reader(captured.string(), roblox.string());
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 2);
    assert(anyTextContains(got, "from the layer"));
    assert(anyTextContains(got, "from roblox"));
}

// A launch that died before Roblox wrote anything. Not an error -- the pane says so instead.
void test_a_missing_roblox_log_is_not_an_error(const fs::path& dir) {
    const fs::path captured = dir / "e-captured.log";
    append(captured, "TuxBlox: only ours\n");
    SessionLogReader reader(captured.string(), "");
    assert(drainAll(reader).size() == 1);
    assert(!reader.robloxLogAppeared());
}

void test_roblox_log_appeared_flips_once_it_has_lines(const fs::path& dir) {
    const fs::path roblox = dir / "f-roblox.log";
    SessionLogReader reader((dir / "f-captured.log").string(), roblox.string());
    drainAll(reader);
    assert(!reader.robloxLogAppeared());
    append(roblox, "2026-10-22T18:00:00.000Z,1.0,03e8,6 [FLog::Output] hello\n");
    drainAll(reader);
    assert(reader.robloxLogAppeared());
}

// Clearing the log, or a rebuild wiping the folder, leaves the reader's position past the end.
void test_a_truncated_file_is_reread_from_the_start(const fs::path& dir) {
    const fs::path captured = dir / "g-captured.log";
    append(captured, "TuxBlox: before the wipe\n");
    SessionLogReader reader(captured.string(), "");
    assert(drainAll(reader).size() == 1);

    std::ofstream(captured, std::ios::trunc).close();
    append(captured, "TuxBlox: after the wipe\n");
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 1);
    assert(anyTextContains(got, "after the wipe"));
}

void test_a_deleted_file_stops_producing_without_throwing(const fs::path& dir) {
    const fs::path captured = dir / "h-captured.log";
    append(captured, "TuxBlox: here\n");
    SessionLogReader reader(captured.string(), "");
    assert(drainAll(reader).size() == 1);
    fs::remove(captured);
    assert(reader.pump().empty());
    assert(reader.pump().empty());
}

// Replaced rather than cleared -- a rebuild writing a fresh file in place. Comparing sizes alone
// misses this whenever the new file is already longer than the old one was.
void test_a_replaced_file_is_reread_from_the_start(const fs::path& dir) {
    const fs::path captured = dir / "k-captured.log";
    append(captured, "TuxBlox: short\n");
    SessionLogReader reader(captured.string(), "");
    assert(drainAll(reader).size() == 1);

    fs::remove(captured);
    append(captured, "TuxBlox: a replacement file that is longer than what it replaced\n");
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 1);
    assert(anyTextContains(got, "a replacement file"));
}

void test_a_partial_last_line_is_held_until_it_is_complete(const fs::path& dir) {
    const fs::path captured = dir / "i-captured.log";
    SessionLogReader reader(captured.string(), "");
    append(captured, "TuxBlox: half a li");
    assert(reader.pump().empty());
    append(captured, "ne\n");
    const std::vector<LogEntry> got = drainAll(reader);
    assert(got.size() == 1);
    assert(anyTextContains(got, "half a line"));
}

// A traceback written across two reads must not come apart: the later lines belong to the entry
// above them. Split off they lose the warning's colour and are hidden outright by "Errors only",
// which filters on an entry's level.
void test_an_entry_split_across_pumps_stays_one_entry(const fs::path& dir) {
    const fs::path captured = dir / "m-captured.log";
    SessionLogReader reader(captured.string(), "");
    append(captured, "2026-10-22T17:52:18.473Z,305.4,03e8,6,Warning [FLog::CreatorWarning] first\n");
    std::vector<LogEntry> all = reader.pump();
    append(captured, " continued on a later read\n");
    const std::vector<LogEntry> later = drainAll(reader);
    all.insert(all.end(), later.begin(), later.end());

    assert(all.size() == 1);
    assert(all[0].level == LogLevel::Warning);
    assert(all[0].continuation.size() == 1);
    assert(all[0].continuation[0].find("later read") != std::string::npos);
}

// The page draws what it is handed and keeps it, so a file that starts over has to be announced --
// otherwise everything already on screen arrives a second time underneath itself.
void test_starting_over_is_announced(const fs::path& dir) {
    const fs::path captured = dir / "n-captured.log";
    append(captured, "TuxBlox: before\n");
    SessionLogReader reader(captured.string(), "");
    assert(drainAll(reader).size() == 1);
    assert(!reader.startedOver());

    std::ofstream(captured, std::ios::trunc).close();
    append(captured, "TuxBlox: after\n");
    reader.pump();
    assert(reader.startedOver());

    // And only on the pump it happened on.
    const std::vector<LogEntry> rest = drainAll(reader);
    assert(!reader.startedOver());
    assert(anyTextContains(rest, "after"));
}

// A wrong timing match has to be visible, so the pane can name what it is actually following.
void test_describe_sources_names_both_files(const fs::path& dir) {
    SessionLogReader reader((dir / "j-captured.log").string(), (dir / "j-roblox.log").string());
    const std::string described = reader.describeSources();
    assert(described.find("j-captured.log") != std::string::npos);
    assert(described.find("j-roblox.log") != std::string::npos);
}

} // namespace

int main() {
    const fs::path dir = fs::temp_directory_path() / "tuxblox_session_log_reader_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    test_returns_nothing_before_anything_is_written(dir);
    test_returns_new_entries_as_they_arrive(dir);
    test_a_second_pump_returns_only_what_is_new(dir);
    test_reads_both_files(dir);
    test_a_missing_roblox_log_is_not_an_error(dir);
    test_roblox_log_appeared_flips_once_it_has_lines(dir);
    test_a_truncated_file_is_reread_from_the_start(dir);
    test_a_deleted_file_stops_producing_without_throwing(dir);
    test_a_replaced_file_is_reread_from_the_start(dir);
    test_a_partial_last_line_is_held_until_it_is_complete(dir);
    test_an_entry_split_across_pumps_stays_one_entry(dir);
    test_starting_over_is_announced(dir);
    test_describe_sources_names_both_files(dir);

    fs::remove_all(dir);
    std::printf("session_log_reader: all tests passed\n");
    return 0;
}
