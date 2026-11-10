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
#include "settings.h"
#include "release_file.h"

namespace tuxblox {

// Which channel's flags this install asks for. The channel the user follows wins:
// switching channel switches everything else in the launcher, so somebody who moved to
// canary and has not updated yet should see canary's flags now rather than after the
// update. The installed release's channel answers only when settings say nothing.
std::string flagChannelFor(const Settings& settings, const RecordedRelease& release);

// Loads the flags for this install. Reads settings and the recorded release itself, so
// the caller has one line rather than four. An install with no recorded release -- every
// local run out of build/ -- makes no request.
void initLauncherFlags(const std::string& installDir);

} // namespace tuxblox
