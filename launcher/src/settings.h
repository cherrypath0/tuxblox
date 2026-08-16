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

struct Settings {
    // Space-separated "VAR=VALUE" pairs, in the same format launch.sh and
    // user_settings.sample.py already use elsewhere in this repo.
    std::string protonEnvVars;  // applied only to the "proton run" child
    std::string globalEnvVars;  // applied to the launcher process itself (setenv), inherited by everything it spawns
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
