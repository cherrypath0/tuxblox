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

#include <algorithm>
#include <cctype>

namespace tuxblox {

namespace {

const char kReplacement[] = "\xEF\xBF\xBD";

bool digits(const std::string& line, size_t from, size_t count) {
    if (from + count > line.size()) return false;
    for (size_t i = from; i < from + count; ++i) {
        if (!std::isdigit(static_cast<unsigned char>(line[i]))) return false;
    }
    return true;
}

// "2026-10-22T18:30:08" at the start, which is how Roblox opens every message it writes.
bool hasRobloxStamp(const std::string& line) {
    if (line.size() < 19) return false;
    return digits(line, 0, 4) && line[4] == '-' && digits(line, 5, 2) && line[7] == '-' &&
           digits(line, 8, 2) && line[10] == 'T' && digits(line, 11, 2) && line[13] == ':' &&
           digits(line, 14, 2) && line[16] == ':' && digits(line, 17, 2);
}

// The compatibility layer's own output: either TuxBlox saying something itself, or Wine's channel form "002c:fixme:ntdll:...".
bool isOurOwnLine(const std::string& line) {
    if (line.rfind("TuxBlox:", 0) == 0) return true;
    const size_t firstColon = line.find(':');
    if (firstColon == std::string::npos || firstColon == 0 || firstColon > 8) return false;
    for (size_t i = 0; i < firstColon; ++i) {
        if (!std::isxdigit(static_cast<unsigned char>(line[i]))) return false;
    }
    const size_t secondColon = line.find(':', firstColon + 1);
    if (secondColon == std::string::npos) return false;
    const std::string channel = line.substr(firstColon + 1, secondColon - firstColon - 1);
    return channel == "fixme" || channel == "err" || channel == "warn" || channel == "trace" ||
           channel == "msg";
}

std::string lowered(const std::string& text) {
    std::string out = text;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

// How many bytes the sequence opening with `lead` should run to, or 0 when it opens nothing valid.
int sequenceLength(unsigned char lead) {
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

LogLevel levelOf(const std::string& line) {
    if (isOurOwnLine(line)) {
        const size_t firstColon = line.find(':');
        if (firstColon != std::string::npos && line.compare(firstColon + 1, 4, "err:") == 0) {
            return LogLevel::Error;
        }
        return LogLevel::Plain;
    }
    // Roblox writes the level as its own comma-separated field, just before the category.
    const size_t category = line.find(" [");
    const std::string head = line.substr(0, category == std::string::npos ? line.size() : category);
    if (head.size() > 6 && head.compare(head.size() - 6, 6, ",Error") == 0) return LogLevel::Error;
    if (head.size() > 8 && head.compare(head.size() - 8, 8, ",Warning") == 0) return LogLevel::Warning;
    return LogLevel::Plain;
}

} // namespace

bool opensAnEntry(const std::string& line) {
    return hasRobloxStamp(line) || isOurOwnLine(line);
}

std::string tidyLine(const std::string& line) {
    if (!hasRobloxStamp(line)) return line;
    const size_t category = line.find(" [");
    if (category == std::string::npos) return line;
    // The time of day is all of the stamp worth keeping; the date is the same for every line on screen.
    return line.substr(11, 12) + line.substr(category);
}

std::string toDisplayableText(const std::string& raw) {
    std::string out;
    out.reserve(raw.size());
    for (size_t at = 0; at < raw.size();) {
        const int length = sequenceLength(static_cast<unsigned char>(raw[at]));
        bool valid = length > 0 && at + static_cast<size_t>(length) <= raw.size();
        for (int i = 1; valid && i < length; ++i) {
            if ((static_cast<unsigned char>(raw[at + i]) & 0xC0) != 0x80) valid = false;
        }
        if (!valid) {
            out += kReplacement;
            ++at;
            continue;
        }
        out.append(raw, at, static_cast<size_t>(length));
        at += static_cast<size_t>(length);
    }
    return out;
}

std::vector<LogEntry> groupLogEntries(const std::vector<std::string>& lines) {
    std::vector<LogEntry> entries;
    for (const std::string& raw : lines) {
        const std::string line = toDisplayableText(raw);
        if (!entries.empty() && !opensAnEntry(line)) {
            entries.back().continuation.push_back(line);
            continue;
        }
        LogEntry entry;
        entry.source = isOurOwnLine(line) ? LogSource::TuxBlox : LogSource::Roblox;
        entry.level = levelOf(line);
        if (hasRobloxStamp(line)) entry.time = line.substr(11, 12);
        entry.text = tidyLine(line);
        // The time is shown in its own column, so it is not repeated in the text beside it.
        if (!entry.time.empty() && entry.text.rfind(entry.time, 0) == 0) {
            entry.text = entry.text.substr(entry.time.size());
            while (!entry.text.empty() && entry.text.front() == ' ') entry.text.erase(0, 1);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

std::vector<LogMatch> findInEntries(const std::vector<LogEntry>& entries, const std::string& query) {
    std::vector<LogMatch> hits;
    if (query.empty()) return hits;
    const std::string needle = lowered(query);

    for (size_t e = 0; e < entries.size(); ++e) {
        const LogEntry& entry = entries[e];
        for (size_t line = 0; line <= entry.continuation.size(); ++line) {
            const std::string& text = line == 0 ? entry.text : entry.continuation[line - 1];
            const std::string hay = lowered(text);
            for (size_t at = hay.find(needle); at != std::string::npos;
                 at = hay.find(needle, at + needle.size())) {
                hits.push_back({e, line, at, needle.size()});
            }
        }
    }
    return hits;
}

} // namespace tuxblox
