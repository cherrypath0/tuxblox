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

#include "studio_presence.h"

#include <sys/stat.h>
#include <utility>

namespace tuxblox {

namespace {

// Collects whole lines out of the follower, which is otherwise built to push them at the presence reader.
class LineCollector : public PresenceReader {
public:
    void consumeLine(const std::string& line) override { lines.push_back(line); }
    PresenceActivity activity() const override { return PresenceActivity(); }

    std::vector<std::string> lines;
};

// What the file is and how big it is now. Both zero when it is not there any more.
void describeFile(const std::string& path, unsigned long long& size, unsigned long long& inode) {
    struct stat status {};
    size = 0;
    inode = 0;
    if (path.empty() || ::stat(path.c_str(), &status) != 0) return;
    size = static_cast<unsigned long long>(status.st_size);
    inode = static_cast<unsigned long long>(status.st_ino);
}

} // namespace

SessionLogReader::SessionLogReader(std::string capturedPath, std::string robloxPath) {
    captured_.path = std::move(capturedPath);
    roblox_.path = std::move(robloxPath);
}

SessionLogReader::~SessionLogReader() = default;

std::vector<std::string> SessionLogReader::drain(Stream& stream) {
    if (stream.path.empty()) return {};

    unsigned long long size = 0;
    unsigned long long inode = 0;
    describeFile(stream.path, size, inode);
    // A file that shrank was cleared, and one with a new identity was replaced. Either way the follower's place in it is meaningless now -- it is holding an offset past the end, or an open handle on a file nobody can see any more -- and it would never produce another line. Size alone misses a replacement that is already longer than what it replaced.
    const bool replaced = stream.inode != 0 && inode != 0 && inode != stream.inode;
    if (size < stream.seen || replaced) {
        stream.tail.reset();
        // Only counts as starting over if there was something to start over from.
        if (stream.produced) startedOver_ = true;
    }
    stream.seen = size;
    stream.inode = inode;

    if (stream.tail == nullptr) {
        if (size == 0) return {};
        stream.tail = std::make_unique<SessionLogTail>(stream.path);
        if (!stream.tail->open()) {
            stream.tail.reset();
            return {};
        }
    }

    LineCollector collector;
    stream.tail->pump(collector);
    if (!collector.lines.empty()) stream.produced = true;
    return std::move(collector.lines);
}

std::vector<LogEntry> SessionLogReader::pump() {
    startedOver_ = false;
    std::vector<std::string> lines = drain(captured_);
    const std::vector<std::string> fromRoblox = drain(roblox_);
    lines.insert(lines.end(), fromRoblox.begin(), fromRoblox.end());
    if (startedOver_) held_.clear();

    // Nothing new means the writer has paused, so whatever was being held is as finished as it is going to get.
    if (lines.empty()) {
        if (held_.empty()) return {};
        std::vector<std::string> complete;
        complete.swap(held_);
        return groupLogEntries(complete);
    }

    std::vector<std::string> all;
    all.swap(held_);
    all.insert(all.end(), lines.begin(), lines.end());

    std::vector<LogEntry> entries = groupLogEntries(all);
    if (entries.empty()) return entries;

    // The last one stays behind: the very next line read could belong to it.
    held_.push_back(entries.back().raw);
    for (const std::string& more : entries.back().rawContinuation) held_.push_back(more);
    entries.pop_back();
    return entries;
}

namespace {

// Just the file's own name. Which file is being read is the point; where the virtual drive keeps it is not something a reader should have to look at.
std::string nameOnly(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

std::string SessionLogReader::describeSources() const {
    if (roblox_.path.empty()) return nameOnly(captured_.path);
    if (captured_.path.empty()) return nameOnly(roblox_.path);
    return nameOnly(captured_.path) + " and " + nameOnly(roblox_.path);
}

bool SessionLogReader::startedOver() const {
    return startedOver_;
}

} // namespace tuxblox
