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

#include "studio_presence.h"
#include "json.hpp"

#include "roblox_log_capture.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <utility>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tuxblox {

namespace {

// Studio names its own state in these two lines and nowhere else, so matching the marker alongside the state avoids catching the same word printed as output.
const char StateMarker[] = "Setting StudioGameStateType to StudioGameStateType_";
const char FallbackMarker[] = "Setting fallback DataModel Standalone";
const char OpenPlaceMarker[] = "[FLog::StudioKeyEvents] open place (identifier = ";

bool contains(const std::string& line, const char *pNeedle) {
    return line.find(pNeedle) != std::string::npos;
}

bool allDigits(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
}

// The place as a person would name it: the file's own name, without the folders around it or the extension after it.
std::string placeNameFromIdentifier(const std::string& identifier) {
    const size_t slash = identifier.find_last_of("/\\");
    std::string leaf = slash == std::string::npos ? identifier : identifier.substr(slash + 1);
    const size_t dot = leaf.find_last_of('.');
    if (dot != std::string::npos && dot > 0) {
        leaf = leaf.substr(0, dot);
    }
    return allDigits(leaf) ? std::string() : leaf;
}

} // namespace

void StudioPresenceReader::consumeLine(const std::string& line) {
    // Only this one line means the session is over. "close IDE doc" closes a tab and the scope exits fire in ordinary use.
    if (contains(line, "exit LifecycleManager UserSession scope")) {
        activity_ = PresenceActivity{};
        return;
    }

    const size_t openPlace = line.find(OpenPlaceMarker);
    if (openPlace != std::string::npos) {
        const size_t start = openPlace + sizeof(OpenPlaceMarker) - 1;
        const size_t end = line.rfind(") [start]");
        if (end != std::string::npos && end > start) {
            activity_.placeName = placeNameFromIdentifier(line.substr(start, end - start));
        }
        return;
    }

    if (contains(line, "[FLog::StudioKeyEvents] team create connect")) {
        activity_.teamCreate = true;
        activity_.kind = PresenceKind::TeamCreate;
        return;
    }

    if (contains(line, "[FLog::StudioKeyEvents] end play test")) {
        activity_.kind = activity_.teamCreate ? PresenceKind::TeamCreate : PresenceKind::Editing;
        return;
    }

    if (contains(line, FallbackMarker)) {
        activity_.kind = PresenceKind::InStudio;
        return;
    }

    if (!contains(line, StateMarker)) {
        return;
    }

    if (contains(line, "StudioGameStateType_PlayClient") ||
        contains(line, "StudioGameStateType_PlayServer")) {
        activity_.kind = PresenceKind::PlayTesting;
    } else if (contains(line, "StudioGameStateType_Edit")) {
        activity_.kind = activity_.teamCreate ? PresenceKind::TeamCreate : PresenceKind::Editing;
    } else if (contains(line, "StudioGameStateType_Standalone")) {
        activity_.kind = PresenceKind::InStudio;
    }
}

PresenceActivity StudioPresenceReader::activity() const {
    return activity_;
}

SessionLogTail::SessionLogTail(std::string path) : path_(std::move(path)) {}

SessionLogTail::~SessionLogTail() {
    if (fd_ >= 0) {
        ::close(fd_);
    }
}

bool SessionLogTail::open() {
    if (fd_ >= 0) {
        return true;
    }
    fd_ = ::open(path_.c_str(), O_RDONLY | O_CLOEXEC | O_NONBLOCK);
    return fd_ >= 0;
}

void SessionLogTail::pump(PresenceReader& reader) {
    if (fd_ < 0 && !open()) {
        return;
    }

    char buffer[8192];
    for (;;) {
        const ssize_t got = ::read(fd_, buffer, sizeof(buffer));
        if (got <= 0) {
            break;
        }
        pending_.append(buffer, static_cast<size_t>(got));
        if (got < static_cast<ssize_t>(sizeof(buffer))) {
            break;
        }
    }

    size_t start = 0;
    for (;;) {
        const size_t newline = pending_.find('\n', start);
        if (newline == std::string::npos) {
            break;
        }
        reader.consumeLine(pending_.substr(start, newline - start));
        start = newline + 1;
    }
    // Whatever is left is a line Studio has not finished writing, so it waits for the next tick.
    pending_.erase(0, start);
}

std::string findStudioSessionLog(const std::string& installDir, std::time_t sessionStart) {
    const std::string dir = robloxLogsDir(installDir);
    std::error_code error;
    std::filesystem::directory_iterator entries(dir, error);
    if (error) {
        return std::string();
    }

    std::vector<std::pair<std::string, std::time_t>> candidates;
    for (const std::filesystem::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name.find("_Studio_") == std::string::npos) {
            continue;
        }
        // stat(), not last_write_time(): the file clock's epoch is unspecified in C++17, and this has to compare against a real time_t. Matches how the session log capture already reads these.
        struct stat status {};
        if (::stat(entry.path().c_str(), &status) != 0) {
            continue;
        }
        candidates.emplace_back(name, status.st_mtime);
    }

    const std::vector<std::string> session = selectSessionLogFiles(candidates, sessionStart);
    if (session.empty()) {
        return std::string();
    }

    // The newest, so a second Studio started in the same session does not pin presence to the first one's log.
    std::string newest = session.front();
    std::time_t newestTime = 0;
    for (const auto& [name, written] : candidates) {
        if (std::find(session.begin(), session.end(), name) != session.end() && written >= newestTime) {
            newestTime = written;
            newest = name;
        }
    }
    return dir + "/" + newest;
}

std::string activityJson(const PresenceActivity& activity, std::time_t startedAt, bool showPlaceName) {
    std::string doing;
    switch (activity.kind) {
        case PresenceKind::Editing: doing = "Editing"; break;
        case PresenceKind::PlayTesting: doing = "Play testing"; break;
        case PresenceKind::TeamCreate: doing = "In Team Create"; break;
        case PresenceKind::InStudio: doing = "In Studio"; break;
        case PresenceKind::None: doing = "In Studio"; break;
    }

    nlohmann::json activityObject;
    // The place is the headline when it is known, with what you are doing under it; otherwise there is just the one line.
    if (showPlaceName && !activity.placeName.empty()) {
        activityObject["details"] = activity.placeName;
    }
    activityObject["state"] = doing;
    activityObject["timestamps"]["start"] = startedAt;

    // A name Roblox wrote in something other than UTF-8 would otherwise throw, and nothing above this catches it.
    return activityObject.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

} // namespace tuxblox
