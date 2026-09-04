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

namespace tuxblox {

// One FastFlag row as the editor holds it. The value is a string whatever the
// flag's type is -- Roblox accepts a string for all of them, so nothing here
// has to know that DFInt takes a number and FFlag takes a boolean.
struct FastFlag {
    std::string name;
    std::string value;
};

// A vector rather than a map, on both counts: the editor renders rows in the
// order they were added, and a half-typed duplicate name has to survive in
// the list while the user is still editing it.
struct FastFlagSet {
    std::vector<FastFlag> player;
    std::vector<FastFlag> studio;
};

struct Settings {
    // Space-separated "VAR=VALUE" pairs, the same format launch.sh uses.
    // Applied two ways, which together cover every process TuxBlox starts:
    // setenv() on the launcher itself, so anything it spawns inherits them,
    // and explicitly on the Proton child, so a launch started directly with
    // --watch-launch (a desktop shortcut) gets them too.
    std::string envVars;
    bool sendCrashReports = true;
    // One of "stable"/"canary"/"dev" -- which /v1/<channel>/... the update
    // checker resolves against. Defaults to "stable" even though it may
    // have no releases published yet (see manifest.h's fetchLatestVersion):
    // that's the correct long-term default for a public installer, not a
    // reflection of what's actually shipped today.
    std::string channel = "stable";
    // Off by default: an available update only shows the Qt UI's side
    // notification popup instead of being applied automatically. See
    // app.h's AppSnapshot::updateAvailableVersion and
    // App::requestUpdateNow() for the rest of that flow.
    bool autoUpdate = false;
    // Written into the active Roblox version's ClientSettings folder at every
    // launch -- see fastflag_file.h for why it can't just be written once.
    FastFlagSet fastFlags;
};

// installDir + "/settings.json". Never throws: a missing file,
// unreadable file, parse error, or malformed/missing fields all fall back
// to a default-constructed Settings{} -- a corrupt settings file must never
// crash the launcher.
Settings loadSettings(const std::string& installDir);
void saveSettings(const std::string& installDir, const Settings& settings);

// Splits a whitespace-separated string of "VAR=VALUE" tokens into
// "VAR=VALUE" strings, tolerant of repeated/extra whitespace. Tokens with
// no '=' are skipped (matches TrackedProcess::start's existing per-pair
// parsing convention in process_launcher.cpp).
std::vector<std::string> parseEnvPairs(const std::string& text);

} // namespace tuxblox
