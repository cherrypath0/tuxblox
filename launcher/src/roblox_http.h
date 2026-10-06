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

// A shared outbound GET, so the certificate bundle, the timeout and the user agent for everything TuxBlox asks Roblox are decided in one place rather than drifting apart in each caller. Empty for an empty address, for any transport failure, and for any answer that is not a plain 200.
std::string httpGet(const std::string& url, long timeoutSeconds = 3);

} // namespace tuxblox
