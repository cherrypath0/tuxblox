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

#include "config.h"
#include <algorithm>
#include <cctype>

namespace tuxblox {

namespace {

const char* const kDefaultServer = "setup.rbxcdn.com";
const char* const kDefaultChannel = "live";
const char* const kVersionsSuffix =
    "/.tuxblox/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";

// An unset variable and one set to nothing mean the same thing here: use the
// default.
std::string readValue(const char* (*readEnv)(const char*), const char* name) {
    const char* value = readEnv(name);
    return value ? std::string(value) : std::string();
}

std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// Only a leading "~" is expanded, which is the form a person types.
std::string expandHome(const std::string& path, const std::string& home) {
    if (path.empty() || path[0] != '~') return path;
    return home + path.substr(1);
}

} // namespace

const char* robloxBinaryType(RobloxApp app) {
    return app == RobloxApp::Player ? "WindowsPlayer" : "WindowsStudio64";
}

Config loadConfig(const char* (*readEnv)(const char*)) {
    Config config;
    const std::string home = readValue(readEnv, "HOME");

    const std::string server = readValue(readEnv, "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_SERVER");
    config.server = server.empty() ? kDefaultServer : server;
    config.serverPinned = !server.empty();

    config.hash = readValue(readEnv, "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXHASH");

    const std::string channel = readValue(readEnv, "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXCHANNEL");
    config.channel = channel.empty() ? kDefaultChannel : toLower(channel);

    // Anything unrecognised stays on the default rather than refusing to run.
    config.app = toLower(readValue(readEnv, "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP")) == "player"
                     ? RobloxApp::Player
                     : RobloxApp::Studio;

    const std::string installDir = readValue(readEnv, "TUXBLOX_BOOTSTRAPPER_INSTALL_DIR");
    config.installDir = installDir.empty() ? home + kVersionsSuffix
                                            : expandHome(installDir, home);
    return config;
}

} // namespace tuxblox
