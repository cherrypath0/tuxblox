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
    // PCI slot of the graphics card to render on, e.g. "0000:01:00.0", or ""
    // for "let the system decide" -- which is the default, and emits no
    // environment at all, so a machine with one graphics card behaves exactly
    // as it did before this setting existed. See system_info.h's GpuDevice for
    // why the slot is stored rather than an index or a name.
    std::string gpu;
    // Controller vibration. On by default, and the default emits no
    // environment at all -- only turning it off does, which is what keeps a
    // launch that never touched this setting exactly as it always was.
    //
    // Roblox drives vibration through XInput, and a PlayStation pad is not an
    // XInput device, so the compatibility layer has to route it through SDL
    // for the motors to be reachable at all. That route costs one button off
    // the pad, which is why this is a setting rather than unconditional.
    bool haptics = true;
    // GPU acceleration for the panels that are web pages -- the login screen
    // and the Toolbox. On by default.
    //
    // Unlike haptics above, BOTH states are emitted: the compatibility layer
    // still treats an absent value as off, so leaving this on has to say so
    // rather than imply it by staying quiet.
    bool webviewGpu = true;
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

// Every "VAR=VALUE" pair a launch should carry, in the order they must be
// applied: the graphics-card selection first, then the user's own Environment
// Variables. That order is the precedence rule -- a variable typed into the
// settings box overrides the same variable coming from the card picker, so
// the box stays the escape hatch for a machine the picker gets wrong.
//
// Defined here rather than at each call site because there are two of them
// (the running launcher and a --watch-launch desktop shortcut) and they must
// not disagree about what a launch's environment is.
std::vector<std::string> launchEnvPairs(const Settings& settings);

} // namespace tuxblox
