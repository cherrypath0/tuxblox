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
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#include <pwd.h>

#include <unistd.h>

namespace tuxblox {

namespace {

const char* const kDefaultServer = "setup.rbxcdn.com";
const char* const kDefaultChannel = "live";
const char* const kPrefixSuffix = "/runtime/pfx";
const char* const kVersionsSuffix = "/AppData/Local/Roblox/Versions";

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
    if (!installDir.empty()) {
        config.installDir = expandHome(installDir, home);
    } else {
        // The launcher normally names the folder. Without it, the bootstrapper sits in the install folder, so its own location is the answer.
        const std::string self = selfExePath();
        const std::size_t slash = self.rfind('/');
        const std::string root = slash == std::string::npos ? home : self.substr(0, slash);
        const std::string prefix = root + kPrefixSuffix;
        config.installDir =
            prefix + "/drive_c/users/" + accountFolderIn(prefix) + kVersionsSuffix;
    }
    return config;
}

std::string hostAccountName() {
    const char *pUser = std::getenv("USER");
    if (pUser && pUser[0] != '\0') return pUser;

    const struct passwd *pEntry = ::getpwuid(::getuid());
    if (pEntry && pEntry->pw_name && pEntry->pw_name[0] != '\0') return pEntry->pw_name;
    return "user";
}

std::string accountFolderIn(const std::string& prefixDir, const std::string& preferred) {
    namespace fs = std::filesystem;

    std::error_code error;
    fs::directory_iterator entries(fs::path(prefixDir) / "drive_c/users", error);
    if (error) return "user";

    std::vector<std::string> found;
    for (const fs::directory_entry& entry : entries) {
        const std::string name = entry.path().filename().string();
        if (name == "Public" || !entry.is_directory(error) || error) continue;
        found.push_back(name);
    }

    if (found.empty()) return "user";
    if (found.size() == 1) return found.front();

    // More than one. Whichever matches this computer's account is the folder %USERPROFILE% names, so it is where Roblox really is.
    if (std::find(found.begin(), found.end(), preferred) != found.end()) return preferred;
    return "user";
}

std::string accountFolderIn(const std::string& prefixDir) {
    return accountFolderIn(prefixDir, hostAccountName());
}

std::string selfExePath() {
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0) return "";
    buf[n] = '\0';
    return std::string(buf);
}

} // namespace tuxblox
