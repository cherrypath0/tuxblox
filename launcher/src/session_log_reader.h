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
#include <memory>
#include <string>
#include <vector>

#include "log_entry.h"

namespace tuxblox {

class SessionLogTail;

// Follows the two files one session writes -- TuxBlox's own capture and Roblox's log -- and hands back whole entries as they appear. They are only joined into one file after the session ends, so while it is running both have to be read at once.
//
// Roblox stamps its lines with a time and TuxBlox does not, so the two cannot be put in order against each other; they are given back in the order they arrived.
class SessionLogReader {
public:
    SessionLogReader(std::string capturedPath, std::string robloxPath);
    ~SessionLogReader();

    SessionLogReader(const SessionLogReader&) = delete;
    SessionLogReader& operator=(const SessionLogReader&) = delete;

    // Whatever is new since the last call, in arrival order.
    std::vector<LogEntry> pump();

    // The files being followed, so a session matched to the wrong log is something a reader can see rather than something they are quietly misled by.
    std::string describeSources() const;

    // True when the last pump started a file over from the beginning, because it was cleared or replaced. Everything already shown is about to arrive again, so the caller has to throw its copy away rather than append to it.
    bool startedOver() const;

    // Whether Roblox's own log has produced anything yet. False means the pane says so instead of looking like a session with nothing to report.
    bool robloxLogAppeared() const;

private:
    struct Stream {
        std::string path;
        std::unique_ptr<SessionLogTail> tail;
        unsigned long long seen = 0;
        unsigned long long inode = 0;
        bool produced = false;
    };

    std::vector<std::string> drain(Stream& stream);

    Stream captured_;
    Stream roblox_;
    // The newest entry, held until something proves it finished. A message that runs over several lines can straddle two reads, and half a traceback handed over as its own entry loses the warning it belongs to and is hidden outright by "Errors only".
    std::vector<std::string> held_;
    bool startedOver_ = false;
};

} // namespace tuxblox
