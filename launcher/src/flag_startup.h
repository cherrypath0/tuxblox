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
#include <vector>
#include "settings.h"
#include "release_file.h"

namespace tuxblox {

// Which channel's flags this install asks for. The channel the user follows wins:
// switching channel switches everything else in the launcher, so somebody who moved to
// canary and has not updated yet should see canary's flags now rather than after the
// update.
//
// `settingsPresent` is load-bearing and not a convenience: loadSettings() defaults the
// channel to "stable", so a missing settings.json is indistinguishable from one that
// chose stable. Without it a fresh canary install -- which has a release file and no
// settings yet -- would silently read stable's flags.
std::string flagChannelFor(const Settings& settings, bool settingsPresent,
                           const RecordedRelease& release);

// True when installDir holds a settings.json at all.
bool settingsFilePresent(const std::string& installDir);

// What the layer needs to read its own flags, for the child environment. Kept out of
// launchEnvVars(), which studio-mcp compiles too and which must stay curl-free: a layer
// started by studio-mcp simply gets neither variable and asks nobody.
std::vector<std::string> flagEnvVars(const std::string& installDir);

// Loads the flags for this install. Reads settings and the recorded release itself, so
// the caller has one line rather than four. An install with no recorded release -- every
// local run out of build/ -- makes no request.
void initLauncherFlags(const std::string& installDir);

// Every flag the launcher honours, in the sense the code wants rather than the sense the
// name is written in: a flag called Disable reads here as the thing being on.
//
// Read once, together, and here rather than at each site. The FIRST read of any flag
// waits for the server's answer, so doing it from a window tick would stall the window;
// this is also the one list of what the launcher actually lets a flag decide.
struct LauncherFlags {
    bool playerButton = false;        // tbxFlagDisablePlayerButton
    bool christmasTheme = false;      // tbxFlagEnableChristmasTheme
    bool geoTool = false;             // tbxFlagDisableGeoTool
    bool crashTelemetry = false;      // tbxFlagDisableCrashTelemetry
    bool robloxNextVersion = false;   // tbxFlagEnableRobloxNextVersionDownload
    bool updater = true;              // tbxFlagDisableUpdater, which only this machine may set
    bool followPromotedBuild = false; // tbxFlagEnableChannelToUpperChannelUpgradeOnBuildPromotion
    // How many of the Roblox installer's downloads run at once. 1 when the flag that
    // enables them is off, so the switch is "parallel or not" and the number is "how much".
    int robloxInstallerWorkers = 1;
    // What the server says the current documents are. Empty when it said nothing, which
    // is an install that has never reached the server and must not be nagged.
    std::string privacyPolicyVersion;  // tbxStringPrivacyPolicyVersion
    std::string termsOfUseVersion;     // tbxStringTermsOfUseVersion
};

// Valid once initLauncherFlags() has run. Reading it before that answers the compiled-in
// defaults, which is what a program that never initialised them should get.
const LauncherFlags& launcherFlags();

} // namespace tuxblox
