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

namespace tuxblox {

enum class PresenceKind {
    None,
    // Studio is open with no place loaded: its start screen, and what closing a place returns to.
    Home,
    Editing,
    PlayTesting,
    // The Player's own states: its app screens, joining a server, in a game, and moving between servers.
    InApp,
    Joining,
    Playing,
    Teleporting,
};

// A field a game set through the RPC protocol. Absent means keep whatever was shown before, and Reset means go back to what TuxBlox shows on its own -- the protocol distinguishes the two and so must this.
struct RpcField {
    // Clear is the protocol's blank string, which drops the field rather than showing an empty one, and Reset is its "<reset>", which hands the field back to TuxBlox.
    enum class State { Absent, Set, Reset, Clear };
    State state = State::Absent;
    std::string value;
};

struct RpcImage {
    RpcField assetId;
    RpcField hoverText;
    // The image behind the asset the game named, once it has been looked up. Empty until then, and an asset nobody could resolve shows nothing rather than something broken.
    std::string resolvedUrl;
};

struct RpcOverride {
    RpcField details;
    RpcField state;
    std::time_t timeStart = 0;
    std::time_t timeEnd = 0;
    RpcImage largeImage;
    RpcImage smallImage;
    // What the game wants carried into a join link, so somebody arriving lands where the player is rather than at the front door.
    std::string launchData;
};

inline bool operator==(const RpcField& a, const RpcField& b) {
    return a.state == b.state && a.value == b.value;
}

inline bool operator==(const RpcImage& a, const RpcImage& b) {
    return a.assetId == b.assetId && a.hoverText == b.hoverText && a.resolvedUrl == b.resolvedUrl;
}

inline bool operator==(const RpcOverride& a, const RpcOverride& b) {
    return a.details == b.details && a.state == b.state && a.timeStart == b.timeStart &&
           a.timeEnd == b.timeEnd && a.largeImage == b.largeImage &&
           a.smallImage == b.smallImage && a.launchData == b.launchData;
}

struct PresenceActivity {
    PresenceKind kind = PresenceKind::None;
    // Empty unless a place is open and its name is known. Shown only when the user has opted in.
    std::string placeName;
    // Set instead of the name when Studio identifies the place by number, which is the usual case for a published place.
    std::string placeId;
    // The place's own icon, once Roblox has been asked for it. Shown in place of the Studio mark.
    std::string placeIconUrl;
    // The place and experience the log reported joining. NOT trustworthy: a game can print any bytes into that log, newlines and a convincing stamp included, so these are only a better-corroborated reading of it and never proof.
    std::string reportedPlaceId;
    std::string universeId;
    // The server itself, which a join link cannot be built without. Only kept when Roblox's own report named the same place, so a game cannot point the link at a server of its choosing.
    std::string jobId;
    // How the experience's author is credited, shown beneath its name the way other launchers do.
    std::string creatorLine;
    // What the game itself asked to be shown, applied over the above.
    RpcOverride rpc;
};

inline bool operator==(const PresenceActivity& a, const PresenceActivity& b) {
    return a.kind == b.kind && a.placeName == b.placeName && a.placeId == b.placeId &&
           a.placeIconUrl == b.placeIconUrl && a.reportedPlaceId == b.reportedPlaceId &&
           a.universeId == b.universeId && a.jobId == b.jobId &&
           a.creatorLine == b.creatorLine && a.rpc == b.rpc;
}

inline bool operator!=(const PresenceActivity& a, const PresenceActivity& b) {
    return !(a == b);
}

// One log line at a time, because that is all any Roblox target gives us. The Player gets its own implementation when it runs.
class PresenceReader {
public:
    virtual ~PresenceReader() = default;
    virtual void consumeLine(const std::string& line) = 0;
    virtual PresenceActivity activity() const = 0;
    // The server this session last connected to, for the region lookup. Empty until one is known, which is also the answer for a reader whose target never reports one.
    virtual std::string serverAddress() const { return std::string(); }
};

} // namespace tuxblox
