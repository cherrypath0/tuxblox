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
#include <optional>
#include <string>

namespace tuxblox {

// Where TuxBlox's own record of the Roblox version history is read from.
//
// Roblox publishes no history: DeployHistory.txt redacts every hash and the
// client-version endpoint only reports what is live now, so the build that came
// before this one cannot be looked up from Roblox at all. config.tuxblox.net is
// what makes installing a previous version possible.

// The base URL for `host`, or the real one when `host` is empty or is not a
// tuxblox.net name. Pure, so the decision is testable without touching the
// environment; configBaseUrl() adds the resolve check around it.
std::string configBaseUrlFor(const char* host);

// configBaseUrlFor(getenv("TUXBLOX_CONFIG_HOST")), resolved once. The allow-list is
// what carries the weight: anything able to set the variable usually cannot also own
// a tuxblox.net name to point it at.
const std::string& configBaseUrl();

// The URL answering "what was live before the current build" for one application on
// one channel. The channel is normalised, so "production" reaches live's history.
std::string configPreviousUrl(const std::string& channel, const std::string& binaryType);

// The hash from a previous-version response, or nothing when the endpoint has no
// earlier build recorded. A 404 body is a reason rather than a build, so it reads as
// nothing instead of throwing.
std::optional<std::string> parsePreviousHash(const std::string& json);

} // namespace tuxblox
