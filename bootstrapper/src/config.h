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

namespace tuxblox {

enum class RobloxApp { Player, Studio };

// "WindowsPlayer" / "WindowsStudio64" -- Roblox's own binaryType strings,
// used both in the version lookup and in DeployHistory.txt lines.
const char* robloxBinaryType(RobloxApp app);

struct Config {
    std::string server;
    // True when the server was named explicitly, which pins it: a run that
    // asked for one mirror should fail rather than quietly use the other.
    bool serverPinned = false;
    // Blank means the newest version on the channel.
    std::string hash;
    std::string channel;
    RobloxApp app = RobloxApp::Studio;
    std::string installDir;
};

// Reads the TUXBLOX_BOOTSTRAPPER_* variables through `readEnv`, filling in
// defaults for anything unset or empty. `readEnv` is a parameter so the
// defaults can be tested without touching the real environment.
Config loadConfig(const char* (*readEnv)(const char*));

// The running binary's real path, via /proc/self/exe rather than argv[0].
std::string selfExePath();

// This computer's account name, as Wine takes it: USER, else the password
// database.
std::string hostAccountName();

// The name of the account folder inside a virtual drive at `prefixDir`, e.g.
// "cherry" in drive_c/users/cherry. The compatibility layer decides it; this
// reads back what it chose. "user" when there is no drive yet.
//
// `preferred` settles it when the drive holds more than one: the one matching
// it is the folder %USERPROFILE% names. Falls back to "user" when none is.
std::string accountFolderIn(const std::string& prefixDir, const std::string& preferred);
std::string accountFolderIn(const std::string& prefixDir);

} // namespace tuxblox
