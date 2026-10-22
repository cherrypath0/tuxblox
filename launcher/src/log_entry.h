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

#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace tuxblox {

enum class LogLevel { Plain, Warning, Error };

// Whether a line came from TuxBlox itself or from Roblox. One session's log holds both, and telling them apart is what lets a reader see which half a problem is in.
enum class LogSource { TuxBlox, Roblox };

// One thing the log has to say. A message that runs over several lines -- a script's traceback, most often -- is one entry, because reading half of one tells you nothing.
struct LogEntry {
    std::string time;
    std::string text;
    std::vector<std::string> continuation;
    // The lines exactly as written. Tidy is a way of looking at an entry, not something done to it, so what it hides has to still be here.
    std::string raw;
    std::vector<std::string> rawContinuation;
    LogLevel level = LogLevel::Plain;
    LogSource source = LogSource::Roblox;
};

// Whether this line begins something new. Roblox stamps the time on the first line of a message and nothing on the rest, and TuxBlox's own lines each stand alone, so anything that is neither continues what came before it.
bool opensAnEntry(const std::string& line);

// The line with the figures only a machine wants -- how long the program had been running, which thread wrote it -- taken out, leaving the time, what wrote it and what it says. Anything that is not in that shape comes back unchanged.
std::string tidyLine(const std::string& line);

// The same text with anything that is not valid UTF-8 replaced, because a text display refuses the whole lot otherwise and one stray byte from the compatibility layer would empty the rest of the log.
std::string toDisplayableText(const std::string& raw);

std::vector<LogEntry> groupLogEntries(const std::vector<std::string>& lines);

// Where one occurrence sits: which entry, which of its lines (0 is the entry's own), and where in that line's displayed text.
struct LogMatch {
    size_t entry = 0;
    size_t line = 0;
    size_t offset = 0;
    size_t length = 0;
};

// Every occurrence of `query`, ignoring case, across an entry and everything that continues it. An empty query finds nothing rather than everything.
std::vector<LogMatch> findInEntries(const std::vector<LogEntry>& entries, const std::string& query);

} // namespace tuxblox
