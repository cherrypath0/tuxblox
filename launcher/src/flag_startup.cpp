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
#include "flags.h"
#include "roblox_config.h"

namespace tuxblox {

namespace {

// Long enough for a slow connection, short enough that nobody notices it failing. The
// program has a perfectly good answer without it.
constexpr int kFlagFetchSeconds = 5;

} // namespace

std::string flagChannelFor(const Settings& settings, const RecordedRelease& release) {
    if (!settings.channel.empty()) return settings.channel;
    if (release.present && !release.file.channel.empty()) return release.file.channel;
    return "stable";
}

void initLauncherFlags(const std::string& installDir) {
    const Settings settings = loadSettings(installDir);
    const RecordedRelease release = readRecordedRelease(installDir);
    initFlags(installDir, flagChannelFor(settings, release), release.present,
              kFlagFetchSeconds, {}, configBaseUrl());
}

} // namespace tuxblox
