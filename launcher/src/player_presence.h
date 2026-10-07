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
#include <string>

#include "presence.h"

namespace tuxblox {

// Reads the Player's own log. Its session state comes from the client's own state machine rather than being inferred from network traffic, because the client says plainly which session it is in.
class PlayerPresenceReader : public PresenceReader {
public:
    void consumeLine(const std::string& line) override;
    PresenceActivity activity() const override;

private:
    PresenceActivity activity_;
    // What the join line claimed, kept only until Roblox's own report either confirms it or does not.
    std::string claimedPlaceId_;
    std::string claimedJobId_;
};

// The place id out of a join line, or empty when it names no published place.
std::string placeIdFromJoinLine(const std::string& line);

// What the log reported joining. Both empty unless the line looks like Roblox's own report, which raises the bar a game has to clear but does not stop it: anything a game can print, it can print convincingly.
struct ReportedJoin {
    std::string placeId;
    std::string universeId;
};

ReportedJoin reportedJoinFromLine(const std::string& line);

// The server's own id out of a join line, or empty unless it is the shape Roblox writes. Checked rather than trusted, because it goes into a link somebody clicks.
std::string jobIdFromJoinLine(const std::string& line);

} // namespace tuxblox
