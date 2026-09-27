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
    if (const char* home = std::getenv("HOME")) {
        assert(!removeInstallDir(home));
    }

    // A relative path can never be right here.
    assert(!removeInstallDir("tuxblox"));

    fs::remove_all(base);
    printf("uninstall_guard: all tests passed\n");
    return 0;
}
