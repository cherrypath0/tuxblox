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

namespace tuxblox {

// What Roblox says about the experience a place belongs to. This is the experience's own name and author, which is what a player recognises -- a place inside it may be called something nobody would know.
struct UniverseInfo {
    std::string name;
    std::string creatorName;
    bool creatorVerified = false;
    // The experience's own place, which is the one its icon belongs to. A lobby or any other place inside it has an icon of its own that nobody would recognise.
    std::string rootPlaceId;
};

// Where Roblox says which experience a place belongs to. Empty for anything that is not a plain number, so a log line can never steer the request somewhere else.
std::string universeLookupUrl(const std::string& placeId);

// The experience's number out of that answer, or empty.
std::string universeIdFromJson(const std::string& body);

// Where Roblox describes the experience itself. Empty for anything that is not a plain number.
std::string universeDetailsUrl(const std::string& universeId);

// The name and author out of that answer. False for anything that is not one, including the placeholders Roblox serves for an experience it will not describe. Never throws: this runs in the presence loop.
bool universeInfoFromJson(const std::string& body, UniverseInfo& into);

// How the author is credited, matching what players already see from other launchers. Empty when Roblox named nobody.
std::string creatorLine(const UniverseInfo& info);

// Asks Roblox about an experience it has already named, which takes one question. False on any failure.
bool fetchUniverseInfoById(const std::string& universeId, UniverseInfo& into);

// Asks Roblox about the experience a place belongs to, which takes two questions. False on any failure.
bool fetchUniverseInfo(const std::string& placeId, UniverseInfo& into);

} // namespace tuxblox
