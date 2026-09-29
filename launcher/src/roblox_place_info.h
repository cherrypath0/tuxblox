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

// Where Roblox describes a place. Empty for anything that is not a plain number, so a log line can never steer the request somewhere else.
std::string placeDetailsUrl(const std::string& placeId);

// The place's name out of that answer, or empty for any answer that does not carry one. Never throws: this runs in the presence loop.
std::string placeNameFromJson(const std::string& body);

// Asks Roblox what a place is called. Empty on any failure, and it gives up quickly rather than holding the loop.
std::string fetchPlaceName(const std::string& placeId);

// Where Roblox serves a place's icon. Empty for anything that is not a plain number.
std::string placeIconsUrl(const std::string& placeId);

// The finished icon out of that answer. A thumbnail Roblox has not rendered, or has blocked, is not one.
std::string placeIconUrlFromJson(const std::string& body);

// Asks Roblox for a place's icon. Empty on any failure.
std::string fetchPlaceIconUrl(const std::string& placeId);

// The place's page on roblox.com. Empty for anything that is not a plain number.
std::string placeGameUrl(const std::string& placeId);

} // namespace tuxblox
