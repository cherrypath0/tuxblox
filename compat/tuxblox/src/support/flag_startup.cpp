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

#include "flag_startup.h"
#include <cstdlib>

namespace tuxblox {

namespace {

// Two seconds, not the five the launcher and the installer use: this runs while somebody
// is waiting for Roblox to start, and a flag is never worth a visible delay.
constexpr int kFlagFetchSeconds = 2;

std::string envOrEmpty(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

} // namespace

void initLayerFlags(const std::string& installDir, FlagFetch fetch) {
    const std::string channel = envOrEmpty("TUXBLOX_FLAGS_CHANNEL");
    // The launcher sets this only for an install that records a release, so an
    // unrecorded one cannot reach the network from here either.
    const bool released = envOrEmpty("TUXBLOX_FLAGS_RELEASED") == "1" && !channel.empty();
    initFlags(installDir, channel, released, kFlagFetchSeconds, std::move(fetch));
}

} // namespace tuxblox
