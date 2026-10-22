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
#include <optional>
#include <string>
#include <vector>

namespace tuxblox {

// This launcher's own path, read from /proc/self/exe, which is the program the
// update check runs. Nothing is returned when that cannot be read -- there is
// deliberately no guessed-path fallback, since an installed launcher may sit at
// either installDir/TuxBloxLauncher or installDir/launcher/TuxBloxLauncher.
std::optional<std::string> ownExePath();

// Where Roblox keeps its installed versions inside the virtual drive.
std::string robloxVersionsDir(const std::string& installDir);

// True when the user has the setting on. The program the check runs is this
// one, so there is nothing else left to be missing.
bool shouldRunUpdateCheck(const Settings& settings);

// True when the check should run and there is an executable to run it with. An
// unreadable /proc/self/exe is not an error: the launch simply goes ahead.
bool canRunUpdateCheck(const Settings& settings, const std::optional<std::string>& exe);

// The "NAME=VALUE" pairs the check is run with.
std::vector<std::string> updateEnvironment(const std::string& installDir, LaunchTarget target);

// Runs the Roblox update check and waits for it, so Roblox is current before
// it starts. Does nothing when the setting is off.
//
// A failure here never stops the launch. Roblox one version behind still
// runs; a Roblox that refuses to start because an update check failed is
// the worse outcome by far.
void runRobloxUpdateCheck(const std::string& installDir, LaunchTarget target,
                           const Settings& settings);
// Makes the version the update just installed the active one, given what was installed before it
// ran. Does nothing when the update installed nothing, so a version chosen on purpose is kept.
void activateNewlyUpdatedVersion(const std::string& installDir, LaunchTarget target,
                                  const std::vector<std::string>& versionsBefore);

// Deletes every other installed version of `target`, plus the archive the download left beside it.
// Only reached on the auto-update path: with that setting on the older builds are not ones the user
// chose to keep, and one a week fills a disk. A version something is still running from is left
// alone, and so is the other app -- Player and Studio share one directory.
void removeSupersededVersions(const std::string& installDir, LaunchTarget target,
                               const std::string& keepHash);

} // namespace tuxblox
