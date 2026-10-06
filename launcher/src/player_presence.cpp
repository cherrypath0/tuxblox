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

// The channel a line was actually written on, which is the first one it names. A game's own output always carries FLog::Output first, so a line that merely quotes another channel's text cannot pass for it.
std::string firstChannel(const std::string& line) {
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

std::string serverAddressFromLine(const std::string& line) {
    if (firstChannel(line) != "[FLog::Network]") {
        return std::string();
    }
    const char marker[] = "[FLog::Network] serverId: ";
    const std::size_t at = line.find(marker);
    if (at == std::string::npos) {
        return std::string();
    }
    const std::size_t start = at + sizeof(marker) - 1;
    const std::size_t bar = line.find('|', start);
    if (bar == std::string::npos) {
        return std::string();
    }
    return line.substr(start, bar - start);
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
        activity_.jobId = jobIdFromJoinLine(line);
        // A teleport is already on its way somewhere, so the join it causes must not read as a fresh one.
        if (activity_.kind != PresenceKind::Teleporting) {
            activity_.kind = PresenceKind::Joining;
        }
        return;
    }

    const std::string address = serverAddressFromLine(line);
    if (!address.empty()) {
        serverAddress_ = address;
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
            activity_.jobId.clear();
            serverAddress_.clear();
            // What the game asked to be shown belonged to that game, and must not follow the user into the next one.
            activity_.rpc = RpcOverride{};
        }
    }
}

PresenceActivity PlayerPresenceReader::activity() const {
    return activity_;
}

std::string PlayerPresenceReader::serverAddress() const {
    return serverAddress_;
}

} // namespace tuxblox
