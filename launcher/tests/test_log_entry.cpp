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

#include "log_entry.h"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

// A real Lua traceback, as Roblox writes it. Only the first line carries a timestamp, and the
// LAST line has no leading space -- so indentation cannot be what decides a continuation.
const std::vector<std::string> kTraceback = {
    "2026-10-22T17:52:18.473Z,305.473083,03e8,6,Warning [FLog::CreatorWarning] Warning: <<< SERVER INFO >>>:  ServerScriptService.Control.PlayerHandlerLobby ",
    " ServerScriptService.Control.PlayerHandlerLobby:267: attempt to index nil with 'GetAttribute' ",
    " ServerScriptService.Control.PlayerHandlerLobby, line 267 - function joinElevator",
    "ServerScriptService.Control.PlayerHandlerLobby, line 355",
    "2026-10-22T17:52:19.001Z,306.001000,03e8,6 [FLog::Output] next entry",
};

const char kPlainRoblox[] =
    "2026-10-22T18:30:08.334Z,399.334290,e9d90000,12 [DFLog::HttpTraceError] CURLOPT_ERRORBUFFER: Connection timed out";
const char kTuxBloxLine[] = "TuxBlox: Running TuxBlox version 3.5.0-stable";
const char kWineFixme[] = "0b94:fixme:ntdll:RtlGetDeviceFamilyInfoEnum 000000000051D378: stub";
const char kWineErr[] = "0024:err:module:import_dll Library not found";

void test_a_timestamped_line_opens_an_entry() {
    assert(opensAnEntry(kPlainRoblox));
    assert(opensAnEntry(kTraceback[0]));
}

// Without this the whole captured file collapses into one entry: TuxBlox's and Wine's own lines
// carry no timestamp, so they would all read as continuations of whatever came first.
void test_a_line_of_ours_opens_an_entry() {
    assert(opensAnEntry(kTuxBloxLine));
    assert(opensAnEntry(kWineFixme));
    assert(opensAnEntry(kWineErr));
}

void test_an_unindented_continuation_still_continues() {
    assert(!opensAnEntry(kTraceback[3]));
    assert(!opensAnEntry(kTraceback[1]));
}

void test_the_traceback_is_one_entry_of_four_lines() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(entries.size() == 2);
    assert(entries[0].continuation.size() == 3);
}

void test_the_next_timestamp_starts_a_new_entry() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(entries[1].continuation.empty());
    assert(entries[1].text.find("next entry") != std::string::npos);
}

// The whole traceback is the warning, not just the line the level was written on.
void test_the_level_covers_the_whole_entry() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(entries[0].level == LogLevel::Warning);
    assert(entries[1].level == LogLevel::Plain);
}

void test_error_and_warning_levels_are_read() {
    const std::vector<LogEntry> entries = groupLogEntries(
        {"2026-10-22T18:00:00.000Z,1.0,03e8,6,Error [FLog::Foo] bad", kPlainRoblox, kWineErr});
    assert(entries[0].level == LogLevel::Error);
    assert(entries[1].level == LogLevel::Plain);
    assert(entries[2].level == LogLevel::Error);
}

void test_tidy_drops_the_uptime_and_thread_id() {
    const std::string tidy = tidyLine(kPlainRoblox);
    assert(tidy.find("18:30:08.334") != std::string::npos);
    assert(tidy.find("[DFLog::HttpTraceError]") != std::string::npos);
    assert(tidy.find("Connection timed out") != std::string::npos);
    // The machine columns are what Tidy exists to remove.
    assert(tidy.find("399.334290") == std::string::npos);
    assert(tidy.find("e9d90000") == std::string::npos);
    assert(tidy.find("2026-10-22") == std::string::npos);
}

// Tidy is a view, not a transformation: the line as written has to survive for Raw to show it.
void test_an_entry_keeps_the_line_as_written() {
    const std::vector<LogEntry> entries = groupLogEntries({kPlainRoblox});
    assert(entries[0].raw == kPlainRoblox);
    assert(entries[0].raw.find("399.334290") != std::string::npos);
    assert(entries[0].text.find("399.334290") == std::string::npos);
}

void test_continuations_keep_their_raw_lines_too() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(entries[0].rawContinuation.size() == entries[0].continuation.size());
    assert(entries[0].rawContinuation[2] == kTraceback[3]);
}

void test_tidy_leaves_a_tuxblox_line_alone() {
    assert(tidyLine(kTuxBloxLine) == kTuxBloxLine);
    assert(tidyLine(kWineFixme) == kWineFixme);
    assert(tidyLine("") == "");
}

void test_a_tuxblox_line_is_tagged_as_ours() {
    const std::vector<LogEntry> entries = groupLogEntries({kTuxBloxLine});
    assert(entries[0].source == LogSource::TuxBlox);
}

void test_a_wine_fixme_line_is_tagged_as_ours() {
    const std::vector<LogEntry> entries = groupLogEntries({kWineFixme});
    assert(entries[0].source == LogSource::TuxBlox);
}

void test_a_roblox_line_is_tagged_as_roblox() {
    const std::vector<LogEntry> entries = groupLogEntries({kPlainRoblox});
    assert(entries[0].source == LogSource::Roblox);
    assert(entries[0].time == "18:30:08.334");
}

// Wine can emit raw bytes. A text widget silently refuses invalid UTF-8, so one bad byte would
// blank the rest of the pane rather than showing one odd character.
void test_invalid_utf8_becomes_replacement_characters() {
    const std::string bad = std::string("ok") + '\xFF' + '\xFE' + "end";
    const std::string shown = toDisplayableText(bad);
    assert(shown.find('\xFF') == std::string::npos);
    assert(shown.find("ok") == 0);
    assert(shown.find("end") != std::string::npos);
    assert(shown.find("\xEF\xBF\xBD") != std::string::npos);
}

// GTK validates with g_utf8_validate and refuses the whole insert, so anything it calls invalid has
// to be replaced here. Lead-byte shape alone is not enough: these all have valid-looking shapes.
void test_overlong_encodings_are_rejected() {
    const char* const overlong[] = {"\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xF0\x80\x80\x80"};
    for (const char* bad : overlong) {
        const std::string shown = toDisplayableText(std::string("a") + bad + "b");
        assert(shown.find("\xEF\xBF\xBD") != std::string::npos);
        assert(shown.find(bad) == std::string::npos);
    }
}

void test_surrogates_are_rejected() {
    const std::string shown = toDisplayableText(std::string("a\xED\xA0\x80z"));
    assert(shown.find("\xEF\xBF\xBD") != std::string::npos);
    assert(shown.find("\xED\xA0\x80") == std::string::npos);
}

void test_values_past_the_last_code_point_are_rejected() {
    for (const std::string bad : {std::string("\xF4\x90\x80\x80"), std::string("\xF5\x80\x80\x80")}) {
        const std::string shown = toDisplayableText("a" + bad + "z");
        assert(shown.find("\xEF\xBF\xBD") != std::string::npos);
        assert(shown.find(bad) == std::string::npos);
    }
}

void test_the_highest_valid_code_point_survives() {
    const std::string highest = "\xF4\x8F\xBF\xBF";
    assert(toDisplayableText(highest) == highest);
}

void test_valid_utf8_is_untouched() {
    const std::string good = "h\xC3\xA9llo \xE2\x9C\x93";
    assert(toDisplayableText(good) == good);
    assert(toDisplayableText("plain ascii") == "plain ascii");
    assert(toDisplayableText("").empty());
}

void test_find_counts_matches_across_continuations() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    const std::vector<LogMatch> hits = findInEntries(entries, "PlayerHandlerLobby");
    // Once on the first line and once on each of the three continuations.
    assert(hits.size() == 4);
    assert(hits[0].entry == 0);
    assert(hits[0].line == 0);
    assert(hits[3].line == 3);
}

void test_find_is_case_insensitive() {
    const std::vector<LogEntry> entries = groupLogEntries({kPlainRoblox});
    assert(findInEntries(entries, "TIMED OUT").size() == 1);
    assert(findInEntries(entries, "timed out").size() == 1);
}

void test_find_with_an_empty_query_matches_nothing() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(findInEntries(entries, "").empty());
}

void test_find_reports_offsets_into_the_displayed_text() {
    const std::vector<LogEntry> entries = groupLogEntries({kPlainRoblox});
    const std::vector<LogMatch> hits = findInEntries(entries, "Connection");
    assert(hits.size() == 1);
    assert(entries[0].text.compare(hits[0].offset, hits[0].length, "Connection") == 0);
}

void test_a_game_output_line_is_its_own_kind() {
    assert(categoryOf("2026-10-22T17:52:19.001Z,306.001000,03e8,6 [FLog::CreatorOutput] hello") ==
           LogCategory::GameOutput);
}

void test_game_output_outranks_the_level_it_carries() {
    // What a game printed belongs with the rest of that game's output, not among the engine's own warnings and errors.
    assert(categoryOf(kTraceback[0]) == LogCategory::GameOutput);
    assert(categoryOf("2026-10-22T17:52:19.001Z,306.001000,03e8,6,Error [FLog::CreatorError] boom") ==
           LogCategory::GameOutput);
}

void test_the_engine_keeps_its_own_errors_and_warnings_apart() {
    assert(categoryOf("2026-10-22T18:30:08.334Z,399.334290,e9d9,12,Error [FLog::Network] gone") ==
           LogCategory::Error);
    assert(categoryOf("2026-10-22T18:30:08.334Z,399.334290,e9d9,12,Warning [FLog::Network] slow") ==
           LogCategory::Warning);
}

void test_an_ordinary_roblox_line_is_info() {
    assert(categoryOf(kPlainRoblox) == LogCategory::Info);
}

void test_our_own_line_is_ours_whatever_it_says() {
    assert(categoryOf(kTuxBloxLine) == LogCategory::TuxBlox);
    assert(categoryOf("0024:err:ntdll:something went wrong") == LogCategory::TuxBlox);
}

void test_an_entry_carries_its_category() {
    const std::vector<LogEntry> entries = groupLogEntries(kTraceback);
    assert(entries.size() == 2);
    assert(entries[0].category == LogCategory::GameOutput);
    assert(entries[1].category == LogCategory::Info);
}

void test_the_filter_starts_hiding_nothing() {
    const LogFilter filter;
    assert(filter.shows(LogCategory::TuxBlox));
    assert(filter.shows(LogCategory::GameOutput));
    assert(filter.shows(LogCategory::Error));
    assert(filter.shows(LogCategory::Warning));
    assert(filter.shows(LogCategory::Info));
}

void test_turning_errors_off_leaves_warnings_alone() {
    LogFilter filter;
    filter.errors = false;
    assert(!filter.shows(LogCategory::Error));
    assert(filter.shows(LogCategory::Warning));
    assert(filter.shows(LogCategory::GameOutput));
}

} // namespace

int main() {
    test_a_timestamped_line_opens_an_entry();
    test_a_line_of_ours_opens_an_entry();
    test_an_unindented_continuation_still_continues();
    test_the_traceback_is_one_entry_of_four_lines();
    test_the_next_timestamp_starts_a_new_entry();
    test_the_level_covers_the_whole_entry();
    test_error_and_warning_levels_are_read();
    test_tidy_drops_the_uptime_and_thread_id();
    test_an_entry_keeps_the_line_as_written();
    test_continuations_keep_their_raw_lines_too();
    test_tidy_leaves_a_tuxblox_line_alone();
    test_a_tuxblox_line_is_tagged_as_ours();
    test_a_wine_fixme_line_is_tagged_as_ours();
    test_a_roblox_line_is_tagged_as_roblox();
    test_invalid_utf8_becomes_replacement_characters();
    test_overlong_encodings_are_rejected();
    test_surrogates_are_rejected();
    test_values_past_the_last_code_point_are_rejected();
    test_the_highest_valid_code_point_survives();
    test_valid_utf8_is_untouched();
    test_find_counts_matches_across_continuations();
    test_find_is_case_insensitive();
    test_find_with_an_empty_query_matches_nothing();
    test_find_reports_offsets_into_the_displayed_text();
    test_a_game_output_line_is_its_own_kind();
    test_game_output_outranks_the_level_it_carries();
    test_the_engine_keeps_its_own_errors_and_warnings_apart();
    test_an_ordinary_roblox_line_is_info();
    test_our_own_line_is_ours_whatever_it_says();
    test_an_entry_carries_its_category();
    test_the_filter_starts_hiding_nothing();
    test_turning_errors_off_leaves_warnings_alone();
    std::printf("log_entry: all tests passed\n");
    return 0;
}
