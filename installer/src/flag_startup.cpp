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
#include "release_stamp.h"
#include "user_agent.h"

namespace tuxblox {

namespace {

constexpr int kFlagFetchSeconds = 5;

} // namespace

void initInstallerFlagsFor(const std::string& installDir, const std::string& channel,
                           bool published, FlagFetch fetch) {
    initFlags(installDir, channel, published, kFlagFetchSeconds, std::move(fetch), {}, userAgent());
}

void initInstallerFlags(const std::string& installDir, const std::string& channel,
                        FlagFetch fetch) {
    // Only a published installer asks: one built locally carries a release from the VERSION file
    // but no channel, because staging is the only thing that can write one.
    initInstallerFlagsFor(installDir, channel, !releaseChannel().empty(), std::move(fetch));
}

} // namespace tuxblox
