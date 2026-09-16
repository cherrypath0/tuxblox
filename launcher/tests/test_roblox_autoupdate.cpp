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

#include "roblox_autoupdate.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;

    // Where the launcher expects the bootstrapper and the versions folder.
    assert(bootstrapperPath("/home/a/.tuxblox") == "/home/a/.tuxblox/TuxBloxBootstrapper");
    assert(robloxVersionsDir("/home/a/.tuxblox") ==
           "/home/a/.tuxblox/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions");

    // The setting decides, but a missing bootstrapper is not an error: an
    // install upgraded from a release that predates it simply has none, and
    // the launch must go ahead anyway.
    {
        Settings on;
        on.autoUpdateRoblox = true;
        assert(shouldRunUpdateCheck(on, /*bootstrapperExists=*/true));
        assert(!shouldRunUpdateCheck(on, /*bootstrapperExists=*/false));

        Settings off;
        off.autoUpdateRoblox = false;
        assert(!shouldRunUpdateCheck(off, /*bootstrapperExists=*/true));
        assert(!shouldRunUpdateCheck(off, /*bootstrapperExists=*/false));
    }

    // The environment handed to the bootstrapper names the app being
    // launched, so --update resolves the right binaryType.
    {
        const auto player = updateEnvironment("/home/a/.tuxblox", LaunchTarget::Player);
        bool sawApp = false, sawDir = false;
        for (const auto& pair : player) {
            if (pair == "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=player") sawApp = true;
            if (pair == "TUXBLOX_BOOTSTRAPPER_INSTALL_DIR=/home/a/.tuxblox/runtime/pfx/drive_c/"
                        "users/user/AppData/Local/Roblox/Versions") sawDir = true;
        }
        assert(sawApp && sawDir);

        const auto studio = updateEnvironment("/home/a/.tuxblox", LaunchTarget::Studio);
        bool sawStudio = false;
        for (const auto& pair : studio) {
            if (pair == "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=studio") sawStudio = true;
        }
        assert(sawStudio);
    }

    printf("roblox_autoupdate: all tests passed\n");
    return 0;
}
