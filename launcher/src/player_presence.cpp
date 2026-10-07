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

#include "player_presence.h"

#include "bloxstrap_rpc.h"

#include <string>

namespace tuxblox {
namespace {

bool contains(const std::string& line, const char *pNeedle) {
    return line.find(pNeedle) != std::string::npos;
}

// Does this line look like one of Roblox's own records, stamped with the time, the uptime and the thread before the channel? This raises the bar a game's output has to clear rather than settling anything: a game can print a stamp of its own just as easily, so nothing read out of this log is ever proof of what happened.
bool hasRecordStamp(const std::string& line) {
    // 2026-10-06T20:31:49.889Z,11.889527,00d4,...
    if (line.size() < 25 || line[4] != '-' || line[7] != '-' || line[10] != 'T' ||
        line[13] != ':' || line[16] != ':') {
        return false;
    }
    const std::size_t z = line.find("Z,");
    if (z == std::string::npos || z > 30) {
        return false;
    }
    for (std::size_t i = 0; i < 4; i++) {
        if (line[i] < '0' || line[i] > '9') {
            return false;
        }
    }
    return true;
}

// The channel a line was actually written on, which is the first one its own record names. A game's own output always carries FLog::Output first, so a line that merely quotes another channel's text cannot pass for it.
std::string firstChannel(const std::string& line) {
    if (!hasRecordStamp(line)) {
        return std::string();
    }
    const std::size_t at = line.find("[FLog::");
    if (at == std::string::npos) {
        return std::string();
    }
    const std::size_t end = line.find(']', at);
    if (end == std::string::npos) {
        return std::string();
    }
    return line.substr(at, end - at + 1);
}

bool allDigits(const std::string& text) {
    if (text.empty()) {
        return false;
    }
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string placeIdFromJoinLine(const std::string& line) {
    const std::size_t at = line.find(" place ");
    if (at == std::string::npos) {
        return std::string();
    }
    const std::size_t start = at + 7;
    std::size_t end = start;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') {
        end++;
    }
    const std::string id = line.substr(start, end - start);
    // A local playtest reports place 0, which is not a place anybody can be sent to, and no real place id is anywhere near twenty digits long.
    if (!allDigits(id) || id == "0" || id.size() > 20) {
        return std::string();
    }
    return id;
}

namespace {

// The digits following a "name:" field in Roblox's own join report.
std::string numberField(const std::string& line, const char *pName) {
    const std::size_t at = line.find(pName);
    if (at == std::string::npos) {
        return std::string();
    }
    std::size_t start = at + std::string(pName).size();
    std::size_t end = start;
    while (end < line.size() && line[end] >= '0' && line[end] <= '9') {
        end++;
    }
    const std::string digits = line.substr(start, end - start);
    return allDigits(digits) && digits.size() <= 20 && digits != "0" ? digits : std::string();
}

} // namespace

ReportedJoin reportedJoinFromLine(const std::string& line) {
    ReportedJoin join;
    if (firstChannel(line) != "[FLog::GameJoinLoadTime]" ||
        line.find("Report game_join_loadtime") == std::string::npos) {
        return join;
    }
    join.placeId = numberField(line, "placeid:");
    join.universeId = numberField(line, "universeid:");
    return join;
}

std::string jobIdFromJoinLine(const std::string& line) {
    const std::size_t at = line.find("Joining game ");
    if (at == std::string::npos) {
        return std::string();
    }
    std::size_t start = at + 13;
    // The Player quotes it and Studio's Team Create line does not.
    if (start < line.size() && line[start] == '\'') {
        start++;
    }
    // Every one Roblox writes is a plain 36-character id, and anything else is refused rather than put in a link.
    const std::size_t JobIdLength = 36;
    if (start + JobIdLength > line.size()) {
        return std::string();
    }
    const std::string id = line.substr(start, JobIdLength);
    for (std::size_t i = 0; i < JobIdLength; i++) {
        const char c = id[i];
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash != (c == '-')) {
            return std::string();
        }
        if (!dash && !((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
            return std::string();
        }
    }
    return id;
}

void PlayerPresenceReader::consumeLine(const std::string& line) {
    if (isBloxstrapRpcLine(line)) {
        applyBloxstrapRpcLine(line, activity_.rpc);
        return;
    }

    const std::string channel = firstChannel(line);
    if (contains(line, "Joining game") &&
        (channel == "[FLog::Output]" || channel == "[FLog::TeamCreateJoinPayload]")) {
        const std::string placeId = placeIdFromJoinLine(line);
        if (!placeId.empty() && placeId != activity_.placeId) {
            activity_.placeId = placeId;
            activity_.placeName.clear();
            activity_.placeIconUrl.clear();
            activity_.creatorLine.clear();
        }
        // Held, not shown: a link is only built once Roblox's own report confirms the same place.
        claimedPlaceId_ = placeId;
        claimedJobId_ = jobIdFromJoinLine(line);
        // A teleport is already on its way somewhere, so the join it causes must not read as a fresh one.
        if (activity_.kind != PresenceKind::Teleporting) {
            activity_.kind = PresenceKind::Joining;
        }
        return;
    }

    const ReportedJoin reported = reportedJoinFromLine(line);
    if (!reported.placeId.empty()) {
        activity_.reportedPlaceId = reported.placeId;
        activity_.universeId = reported.universeId;
        activity_.jobId = claimedPlaceId_ == reported.placeId ? claimedJobId_ : std::string();
        return;
    }

    if (firstChannel(line) == "[FLog::SessionTransitionFSM]") {
        if (contains(line, "Entered play session")) {
            activity_.kind = PresenceKind::Playing;
        } else if (contains(line, "Teleported")) {
            activity_.kind = PresenceKind::Teleporting;
        } else if (contains(line, "Entered app session") || contains(line, "Tearing down")) {
            activity_.kind = PresenceKind::InApp;
            activity_.placeId.clear();
            activity_.placeName.clear();
            activity_.placeIconUrl.clear();
            activity_.creatorLine.clear();
            activity_.reportedPlaceId.clear();
            activity_.universeId.clear();
            activity_.jobId.clear();
            claimedPlaceId_.clear();
            claimedJobId_.clear();
            // What the game asked to be shown belonged to that game, and must not follow the user into the next one.
            activity_.rpc = RpcOverride{};
        }
    }
}

PresenceActivity PlayerPresenceReader::activity() const {
    return activity_;
}

} // namespace tuxblox
