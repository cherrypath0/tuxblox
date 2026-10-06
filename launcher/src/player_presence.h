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
    std::string serverAddress() const override;

private:
    PresenceActivity activity_;
    std::string serverAddress_;
};

// The place id out of a join line, or empty when it names no published place.
std::string placeIdFromJoinLine(const std::string& line);

// The address out of a serverId line, or empty if the line is not one.
std::string serverAddressFromLine(const std::string& line);

} // namespace tuxblox
