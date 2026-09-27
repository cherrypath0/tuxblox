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
#include <cassert>
#include <filesystem>
#include <cstdio>
#include <map>
#include <string>

namespace fs = std::filesystem;

namespace {
// Stands in for the real environment so the defaults can be tested without
// touching the process's own.
std::map<std::string, std::string> g_env;
const char* fakeGetenv(const char* name) {
    auto it = g_env.find(name);
    return it == g_env.end() ? nullptr : it->second.c_str();
}
} // namespace

int main() {
    using namespace tuxblox;

    // Nothing set: every value falls back, and the versions folder is found
    // from the bootstrapper's own location, so an install outside the home
    // folder still works.
    {
        g_env = {{"HOME", "/home/someone"}};
        Config c = loadConfig(fakeGetenv);
        assert(c.server == "setup.rbxcdn.com");
        assert(!c.serverPinned);
        assert(c.hash.empty());
        assert(c.channel == "live");
        assert(c.app == RobloxApp::Studio);

        const std::string self = selfExePath();
        assert(!self.empty() && self[0] == '/');
        const std::string root = self.substr(0, self.rfind('/'));
        const std::string prefix = root + "/runtime/pfx";
        assert(c.installDir == prefix + "/drive_c/users/" + accountFolderIn(prefix) +
                                   "/AppData/Local/Roblox/Versions");
    }

    // The fallback follows the drive's real account folder too, so a run
    // without the launcher naming the folder still lands where Roblox is read
    // from rather than in a folder nobody looks at.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_bootstrap_account";
        fs::remove_all(base);
        fs::create_directories(base / "drive_c/users/cherry");
        assert(accountFolderIn(base.string(), "cherry") == "cherry");
        fs::create_directories(base / "drive_c/users/Public");
        assert(accountFolderIn(base.string(), "cherry") == "cherry");
        fs::create_directories(base / "drive_c/users/user");
        // Two of them: follow the one matching this computer's account, since
        // that is the folder %USERPROFILE% names and where Roblox really is.
        assert(accountFolderIn(base.string(), "cherry") == "cherry");
        assert(accountFolderIn(base.string(), "someone-else") == "user");
        fs::remove_all(base);
        assert(accountFolderIn(base.string(), "cherry") == "user"); // no drive yet
    }

    // Each variable overrides its default.
    {
        g_env = {{"HOME", "/home/someone"},
                 {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_SERVER", "setup-aws.rbxcdn.com"},
                 {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXHASH", "version-abc123"},
                 {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_RBXCHANNEL", "ZCanary"},
                 {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP", "player"},
                 {"TUXBLOX_BOOTSTRAPPER_INSTALL_DIR", "/tmp/versions"}};
        Config c = loadConfig(fakeGetenv);
        assert(c.server == "setup-aws.rbxcdn.com");
        // Naming a server pins it: no silent fallback to the other mirror.
        assert(c.serverPinned);
        assert(c.hash == "version-abc123");
        // Channel names are lowercase on the CDN.
        assert(c.channel == "zcanary");
        assert(c.app == RobloxApp::Player);
        assert(c.installDir == "/tmp/versions");
    }

    // A leading ~ in the install directory is the user's home.
    {
        g_env = {{"HOME", "/home/someone"},
                 {"TUXBLOX_BOOTSTRAPPER_INSTALL_DIR", "~/elsewhere/Versions"}};
        assert(loadConfig(fakeGetenv).installDir == "/home/someone/elsewhere/Versions");
    }

    // App name is read loosely, and anything unrecognised stays on the
    // default rather than failing a launch.
    {
        g_env = {{"HOME", "/h"}, {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP", "Player"}};
        assert(loadConfig(fakeGetenv).app == RobloxApp::Player);
        g_env = {{"HOME", "/h"}, {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP", "banana"}};
        assert(loadConfig(fakeGetenv).app == RobloxApp::Studio);
    }

    // An empty value is not an override.
    {
        g_env = {{"HOME", "/h"}, {"TUXBLOX_BOOTSTRAPPER_DOWNLOAD_SERVER", ""}};
        Config c = loadConfig(fakeGetenv);
        assert(c.server == "setup.rbxcdn.com");
        assert(!c.serverPinned);
    }

    // The binaryType strings Roblox itself uses.
    assert(std::string(robloxBinaryType(RobloxApp::Player)) == "WindowsPlayer");
    assert(std::string(robloxBinaryType(RobloxApp::Studio)) == "WindowsStudio64");

    printf("config: all tests passed\n");
    return 0;
}
