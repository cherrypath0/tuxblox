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
#include "lnk_resolver.h"
#include "settings.h"
#include <string>
#include <vector>

namespace tuxblox {

// installDir + "/TuxBloxBootstrapper".
std::string bootstrapperPath(const std::string& installDir);

// Where Roblox keeps its installed versions inside the virtual drive.
std::string robloxVersionsDir(const std::string& installDir);

// True only when the user has the setting on and a bootstrapper is actually
// there. An install upgraded from a release that predates the bootstrapper
// has none, which is not an error -- the launch simply goes ahead.
bool shouldRunUpdateCheck(const Settings& settings, bool bootstrapperExists);

// The "NAME=VALUE" pairs the bootstrapper is run with.
std::vector<std::string> updateEnvironment(const std::string& installDir, LaunchTarget target);

// Runs the bootstrapper's update check and waits for it, so Roblox is
// current before it starts. Does nothing when the setting is off or no
// bootstrapper is installed.
//
// A failure here never stops the launch. Roblox one version behind still
// runs; a Roblox that refuses to start because an update check failed is
// the worse outcome by far.
void runRobloxUpdateCheck(const std::string& installDir, LaunchTarget target,
                           const Settings& settings);

} // namespace tuxblox
