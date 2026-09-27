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

#include "uninstall.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    const fs::path base = fs::temp_directory_path() / "tuxblox_test_uninstall_guard";
    fs::remove_all(base);

    // A directory that is not a TuxBlox install is never deleted, however
    // confidently it was named.
    {
        const fs::path dir = base / "not-an-install";
        fs::create_directories(dir);
        std::ofstream(dir / "someones-homework.txt").put('x');
        assert(!removeInstallDir(dir.string()));
        assert(fs::exists(dir / "someones-homework.txt"));
    }

    // One that is, is.
    {
        const fs::path dir = base / "an-install";
        fs::create_directories(dir);
        std::ofstream(dir / "TuxBloxLauncher").put('x');
        assert(removeInstallDir(dir.string()));
        assert(!fs::exists(dir));
    }

    // A path that was never there counts as already removed.
    assert(removeInstallDir((base / "never-existed").string()));

    // The roots that a bad value would take the machine with.
    assert(!removeInstallDir("/"));
    assert(!removeInstallDir("/opt"));
    assert(!removeInstallDir("//"));
    assert(!removeInstallDir("/opt/"));
    // The home folder itself, however it is spelled. A real home is two
    // components deep, so the depth check does not save it -- set one rather
    // than trusting whatever HOME happens to be where this test runs.
    {
        const fs::path fakeHome = base / "home" / "someone";
        fs::create_directories(fakeHome);
        // Planted by a user who installed into their home folder by mistake,
        // which is what makes the marker check stop being a barrier.
        std::ofstream(fakeHome / "TuxBloxLauncher").put('x');
        setenv("HOME", fakeHome.c_str(), 1);

        assert(!removeInstallDir(fakeHome.string()));
        // Shell tab-completion appends the separator, and "/." is the same
        // folder written another way. Neither may get past the home check.
        assert(!removeInstallDir(fakeHome.string() + "/"));
        assert(!removeInstallDir(fakeHome.string() + "/."));
        assert(!removeInstallDir(fakeHome.string() + "//"));
        assert(fs::exists(fakeHome / "TuxBloxLauncher"));

        // A real install inside it is still removable.
        const fs::path inside = fakeHome / ".tuxblox";
        fs::create_directories(inside);
        std::ofstream(inside / "TuxBloxLauncher").put('x');
        assert(removeInstallDir(inside.string() + "/"));
        assert(!fs::exists(inside));
    }

    // A relative path can never be right here.
    assert(!removeInstallDir("tuxblox"));

    fs::remove_all(base);
    printf("uninstall_guard: all tests passed\n");
    return 0;
}
