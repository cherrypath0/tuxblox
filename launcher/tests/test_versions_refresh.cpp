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

// Checks that a running launcher notices a Roblox version installed by the bootstrapper, which runs
// as its own process and so cannot tell the launcher anything.

#include "app.h"
#include "versions_manifest.h"

#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace fs = std::filesystem;

namespace {

fs::path versionsDirIn(const fs::path& installDir) {
    return installDir / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
}

void installVersion(const fs::path& installDir, const std::string& hash) {
    const fs::path dir = versionsDirIn(installDir) / hash;
    fs::create_directories(dir);
    std::ofstream(dir / "RobloxStudioBeta.exe") << "not a windows program";
}

// The poll runs once a second, so this allows a few turns before giving up.
bool waitForVersionCount(tuxblox::App& app, size_t expected) {
    for (int waited = 0; waited < 6000; waited += 100) {
        if (app.snapshot().versions.studio.installed.size() == expected) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

} // namespace

int main() {
    const fs::path work = fs::temp_directory_path() / "tuxblox_test_versions_refresh";
    fs::remove_all(work);
    const fs::path installDir = work / "install";
    fs::create_directories(installDir);

    installVersion(installDir, "version-first");

    tuxblox::App app(installDir.string(), "0.0.0-test", (work / "launcher").string());

    // What the launcher knew when its window opened.
    assert(app.snapshot().versions.studio.installed.size() == 1);
    assert(app.snapshot().versions.studio.activeHash == "version-first");
    printf("  at startup: 1 version, active version-first\n");

    // The bootstrapper installs a second one from its own process and tells nobody.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    installVersion(installDir, "version-second");

    assert(waitForVersionCount(app, 2));
    printf("  after an outside install: %zu versions\n",
           app.snapshot().versions.studio.installed.size());

    // With nothing recorded as chosen, the newest installed version is the active one, which is
    // what makes an update show up as in use rather than the version it replaced.
    assert(app.snapshot().versions.studio.activeHash == "version-second");
    printf("  active followed the new install: version-second\n");

    // And a version removed from outside disappears again.
    fs::remove_all(versionsDirIn(installDir) / "version-second");
    assert(waitForVersionCount(app, 1));
    printf("  after an outside removal: 1 version\n");

    // Once a version is recorded as chosen, a third install must not move it.
    {
        tuxblox::VersionsManifest chosen = tuxblox::loadInstalledVersions(installDir.string());
        chosen.studio.activeHash = "version-first";
        tuxblox::saveVersionsManifest(installDir.string(), chosen);

        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        installVersion(installDir, "version-third");
        assert(waitForVersionCount(app, 2));
        assert(app.snapshot().versions.studio.activeHash == "version-first");
        printf("  a recorded choice survives a later outside install\n");
    }

    fs::remove_all(work);
    printf("versions_refresh: all tests passed\n");
    return 0;
}
