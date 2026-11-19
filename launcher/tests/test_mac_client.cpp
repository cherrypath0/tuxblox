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

#include "mac_client.h"
#include "mac_signature.h"
#include "session_list.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox::macclient;

    // The upload becomes a directory name and is deleted on update, so only
    // a strict version-<hex> shape is accepted.
    {
        assert(validUpload("version-445548190fe44ee2"));
        assert(validUpload("version-ABCDEF0123"));
        assert(!validUpload("version-"));
        assert(!validUpload("version-xyz"));
        assert(!validUpload("445548190fe44ee2"));
        assert(!validUpload("version-445548190fe44ee2/../../etc"));
    }

    {
        const std::string upload = "version-445548190fe44ee2";
        assert(setupUrl("https://setup.rbxcdn.com", "", upload) ==
               "https://setup.rbxcdn.com/mac/version-445548190fe44ee2-RobloxPlayer.zip");
        assert(setupUrl("https://setup.rbxcdn.com/", "zbeta", upload) ==
               "https://setup.rbxcdn.com/channel/zbeta/common/mac/"
               "version-445548190fe44ee2-RobloxPlayer.zip");
        assert(setupUrl("https://setup.rbxcdn.com", "live", upload) ==
               "https://setup.rbxcdn.com/mac/version-445548190fe44ee2-RobloxPlayer.zip");
    }

    {
        const auto args = playerLaunchArgs("/opt/game/RobloxPlayer", "roblox-player://join?ticket=x");
        assert(args.size() == 4);
        assert(args[0] == "run");
        assert(args[1] == "/opt/game/RobloxPlayer");
        assert(args[2] == "-protocolString");
        assert(args[3] == "roblox-player://join?ticket=x");
        assert(playerLaunchArgs("/opt/game/RobloxPlayer", "").size() == 2);
    }

    // An install with no marker has nothing to reuse and no integrity.
    {
        const fs::path root = fs::temp_directory_path() / "tuxblox_test_mac_client";
        fs::remove_all(root);
        fs::create_directories(root);
        Build out;
        assert(!installedBuild(root.string(), out));
        assert(!integrityIntact(root.string()));
        assert(playerRoot(root.string()) == (root / "mac-client" / "player").string());
        fs::remove_all(root);
    }

    // A directory that is not a signed bundle is refused, not accepted.
    {
        const fs::path root = fs::temp_directory_path() / "tuxblox_test_mac_signature";
        fs::remove_all(root);
        fs::create_directories(root / "RobloxPlayer.app" / "Contents");
        std::ofstream(root / "RobloxPlayer.app" / "Contents" / "Info.plist") << "<plist/>";
        assert(!verifyAppSignature((root / "RobloxPlayer.app").string(),
                                   "com.roblox.RobloxPlayer").empty());
        assert(codeDirectoryHash((root / "RobloxPlayer.app").string()).empty());
        fs::remove_all(root);
    }

    // The session census finds a marked Rosemary process by its env, and
    // leaves a process from another install (or without the mark) alone.
    {
        const fs::path proc = fs::temp_directory_path() / "tuxblox_test_mac_proc";
        const fs::path install = "/tmp/tuxblox_test_mac_install";
        fs::remove_all(proc);
        fs::create_directories(proc / "4242");
        fs::create_directories(proc / "4243");
        std::ofstream(proc / "stat") << "btime 1\n";
        // 22 space-separated fields after the comm, the last one being starttime.
        const std::string statLine =
            "4242 (rosemary) S 1 1 1 0 -1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 100\n";
        std::ofstream(proc / "4242" / "stat") << statLine;
        std::ofstream(proc / "4242" / "environ", std::ios::binary)
            << std::string("TUXBLOX_MAC_CLIENT=1", 20) << '\0'
            << "ROSEMARY_PREFIX=/tmp/tuxblox_test_mac_install/mac-client/prefix" << '\0';
        const std::string cmdline =
            std::string("rosemary", 8) + '\0' + "run" + '\0' +
            "/tmp/tuxblox_test_mac_install/mac-client/player/version-abc/"
            "RobloxPlayer.app/Contents/MacOS/RobloxPlayer" + '\0';
        std::ofstream(proc / "4242" / "cmdline", std::ios::binary)
            .write(cmdline.data(), static_cast<std::streamsize>(cmdline.size()));
        std::ofstream(proc / "4243" / "environ", std::ios::binary)
            << std::string("TUXBLOX_MAC_CLIENT=1", 20) << '\0'
            << "ROSEMARY_PREFIX=/tmp/another-install/mac-client/prefix" << '\0';

        const auto sessions = tuxblox::macSessionsIn(proc.string(), install.string());
        assert(sessions.size() == 1);
        assert(sessions[0].pid == 4242);
        assert(sessions[0].target == tuxblox::LaunchTarget::Player);
        fs::remove_all(proc);
    }

    return 0;
}
