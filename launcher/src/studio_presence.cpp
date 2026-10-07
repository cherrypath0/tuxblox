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

#include "bloxstrap_rpc.h"
#include "player_presence.h"
#include "json.hpp"
#include "roblox_place_info.h"

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
// The image uploaded against the Discord application. Discord drops it silently if it is not there, which costs only the artwork.
const char StudioAssetKey[] = "studio";
// The Player's own mark, uploaded to the application a Player session reports under.
const char PlayerAssetKey[] = "robloxplayer";

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
            const std::string identifier = line.substr(start, end - start);
            const size_t slash = identifier.find_last_of("/\\");
            const std::string leaf = slash == std::string::npos ? identifier
                                                                : identifier.substr(slash + 1);
            activity_.placeId = allDigits(leaf) ? leaf : std::string();
            activity_.placeName = placeNameFromIdentifier(identifier);
        }
        return;
    }

    // Team Create is not a state of its own: editing a place with other people in it is still editing a place.
    if (contains(line, "[FLog::StudioKeyEvents] end play test")) {
        activity_.kind = PresenceKind::Editing;
        return;
    }

    if (contains(line, FallbackMarker)) {
        activity_ = PresenceActivity{};
        activity_.kind = PresenceKind::Home;
        return;
    }

    if (!contains(line, StateMarker)) {
        return;
    }

    if (contains(line, "StudioGameStateType_PlayClient") ||
        contains(line, "StudioGameStateType_PlayServer")) {
        activity_.kind = PresenceKind::PlayTesting;
    } else if (contains(line, "StudioGameStateType_Edit")) {
        activity_.kind = PresenceKind::Editing;
    } else if (contains(line, "StudioGameStateType_Null")) {
        // No datamodel is what closing a place leaves behind, which is the home page with nothing open.
        activity_ = PresenceActivity{};
        activity_.kind = PresenceKind::Home;
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

std::string findSessionLog(const std::string& installDir, std::time_t sessionStart, bool player) {
    const std::string dir = robloxLogsDir(installDir);
    std::error_code error;
    std::filesystem::directory_iterator entries(dir, error);
    if (error) {
        return std::string();
    }

    std::vector<std::pair<std::string, std::time_t>> candidates;
    for (const std::filesystem::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name.find(player ? "_Player_" : "_Studio_") == std::string::npos) {
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

namespace {

void applyRpcImage(nlohmann::json& activityObject, const std::string& slot, const RpcImage& image) {
    const std::string key = slot + "_image";
    const std::string textKey = slot + "_text";
    // Nothing may have put art there at all, and erasing from a value that was never made throws.
    const bool haveAssets = activityObject.contains("assets") && activityObject["assets"].is_object();

    if (image.assetId.state == RpcField::State::Clear) {
        if (haveAssets) {
            activityObject["assets"].erase(key);
            activityObject["assets"].erase(textKey);
        }
        return;
    }
    if (image.assetId.state == RpcField::State::Reset) {
        return;
    }
    // An asset that has not been looked up yet, or that Roblox had no picture for, shows nothing rather than an address Discord cannot load.
    if (image.assetId.state == RpcField::State::Set && !image.resolvedUrl.empty()) {
        activityObject["assets"][key] = image.resolvedUrl;
    }
    if (image.hoverText.state == RpcField::State::Set) {
        activityObject["assets"][textKey] = image.hoverText.value;
    } else if (image.hoverText.state == RpcField::State::Clear && haveAssets) {
        activityObject["assets"].erase(textKey);
    }
}

} // namespace

std::string activityJson(const PresenceActivity& activity, std::time_t startedAt) {
    const bool named = !activity.placeName.empty();
    std::string phrase;
    switch (activity.kind) {
        case PresenceKind::Editing:
            phrase = named ? "Editing " + activity.placeName : "Editing a place";
            break;
        case PresenceKind::PlayTesting:
            phrase = named ? "Playtesting " + activity.placeName : "Playtesting a place";
            break;
        case PresenceKind::Home:
        case PresenceKind::None:
            phrase = "In the home page";
            break;
        case PresenceKind::Playing:
            phrase = named ? "Playing " + activity.placeName : "Playing a game";
            break;
        case PresenceKind::Joining:
            phrase = named ? "Joining " + activity.placeName : "Joining a game";
            break;
        case PresenceKind::Teleporting:
            phrase = "Moving to another server";
            break;
        case PresenceKind::InApp:
            phrase = "In the Roblox app";
            break;
    }

    const bool studio = activity.kind == PresenceKind::Editing ||
                        activity.kind == PresenceKind::PlayTesting ||
                        activity.kind == PresenceKind::Home || activity.kind == PresenceKind::None;

    nlohmann::json activityObject;
    // A Player session reads the way players already know it from other launchers: the experience's own name, and its author beneath rather than a sentence about it.
    activityObject["details"] = studio || !named ? phrase : activity.placeName;
    if (!studio && !activity.creatorLine.empty()) {
        activityObject["state"] = activity.creatorLine;
    }
    activityObject["timestamps"]["start"] = startedAt;

    // Only a published place has a page to send anyone to, and for the Player only the place Roblox
    // itself reported joining -- a game writes to the same log, so a link must never come from a line
    // it could have written.
    const std::string gameUrl =
        placeGameUrl(studio ? activity.placeId : activity.reportedPlaceId);
    if (!gameUrl.empty() && studio) {
        activityObject["buttons"] = nlohmann::json::array(
            {{{"label", "Open Game Link"}, {"url", gameUrl}}});
    } else if (!gameUrl.empty()) {
        activityObject["buttons"] = nlohmann::json::array();
        // Joining needs a server as well as a place, and a player on the home screen is in neither.
        const std::string joinUrl =
            placeJoinUrl(activity.reportedPlaceId, activity.jobId, activity.rpc.launchData);
        if (!joinUrl.empty()) {
            activityObject["buttons"].push_back({{"label", "Join server"}, {"url", joinUrl}});
        }
        activityObject["buttons"].push_back({{"label", "See game page"}, {"url", gameUrl}});
    }

    // The place's own icon when there is one, with the program's mark badged onto it; otherwise the mark alone.
    const char *pMark = studio ? StudioAssetKey : PlayerAssetKey;
    const char *pMarkText = studio ? "Roblox Studio" : "Roblox";
    if (named && !activity.placeIconUrl.empty()) {
        activityObject["assets"]["large_image"] = activity.placeIconUrl;
        activityObject["assets"]["large_text"] = activity.placeName;
        activityObject["assets"]["small_image"] = pMark;
        activityObject["assets"]["small_text"] = pMarkText;
    } else {
        activityObject["assets"]["large_image"] = pMark;
        activityObject["assets"]["large_text"] = pMarkText;
    }

    // Applied last, so a game's own text and art sit on top of what TuxBlox worked out, and a reset leaves what is already above untouched.
    if (activity.rpc.details.state == RpcField::State::Set) {
        activityObject["details"] = activity.rpc.details.value;
    } else if (activity.rpc.details.state == RpcField::State::Clear) {
        activityObject.erase("details");
    }
    if (activity.rpc.state.state == RpcField::State::Set) {
        activityObject["state"] = activity.rpc.state.value;
    } else if (activity.rpc.state.state == RpcField::State::Clear) {
        activityObject.erase("state");
    }
    if (activity.rpc.timeStart > 0) {
        activityObject["timestamps"]["start"] = activity.rpc.timeStart;
    }
    if (activity.rpc.timeEnd > 0) {
        activityObject["timestamps"]["end"] = activity.rpc.timeEnd;
    }
    applyRpcImage(activityObject, "large", activity.rpc.largeImage);
    applyRpcImage(activityObject, "small", activity.rpc.smallImage);

    // A name Roblox wrote in something other than UTF-8 would otherwise throw, and nothing above this catches it.
    return activityObject.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
}

std::string findStudioSessionLog(const std::string& installDir, std::time_t sessionStart) {
    return findSessionLog(installDir, sessionStart, false);
}

bool rpcAllowsSend(std::vector<std::time_t>& recentSends, std::time_t now) {
    const std::time_t WindowSeconds = 20;
    const std::size_t MaxInWindow = 5;
    recentSends.erase(std::remove_if(recentSends.begin(), recentSends.end(),
                                     [now](std::time_t at) { return now - at >= WindowSeconds; }),
                      recentSends.end());
    return recentSends.size() < MaxInWindow;
}

} // namespace tuxblox
