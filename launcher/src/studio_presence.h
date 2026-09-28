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
#include <ctime>
#include <string>

#include "presence.h"

namespace tuxblox {

class StudioPresenceReader : public PresenceReader {
public:
    void consumeLine(const std::string& line) override;
    PresenceActivity activity() const override;

private:
    PresenceActivity activity_;
};

// Follows a file that is still being written, handing over only whole lines. The offset is held so each tick costs one read of whatever is new.
class SessionLogTail {
public:
    explicit SessionLogTail(std::string path);
    ~SessionLogTail();

    SessionLogTail(const SessionLogTail&) = delete;
    SessionLogTail& operator=(const SessionLogTail&) = delete;

    bool open();
    void pump(PresenceReader& reader);

private:
    std::string path_;
    int fd_ = -1;
    std::string pending_;
};

// Studio's own log for this session, or empty if it has not appeared yet.
std::string findStudioSessionLog(const std::string& installDir, std::time_t sessionStart);

// Discord's activity object. The heading above it comes from the Application ID and cannot be set here.
std::string activityJson(const PresenceActivity& activity, std::time_t startedAt, bool showPlaceName);

} // namespace tuxblox
