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

} // namespace tuxblox
