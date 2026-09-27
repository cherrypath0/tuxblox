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

#include "install_paths.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    using tuxblox::installDir;
    using tuxblox::hasEnoughDiskSpace;
    using tuxblox::looksLikeInstall;
    using tuxblox::selfExePath;
    using tuxblox::existingAncestor;

    // Not in an install directory: the default is still the home folder, so
    // a freshly downloaded installer behaves exactly as it always has.
    unsetenv("TUXBLOX_ROOT");
    setenv("HOME", "/tmp/tuxblox_test_home", 1);
    assert(installDir() == "/tmp/tuxblox_test_home/.tuxblox");

    // TUXBLOX_ROOT wins, trailing slashes and all.
    setenv("TUXBLOX_ROOT", "/opt/tuxblox/", 1);
    assert(installDir() == "/opt/tuxblox");
    setenv("TUXBLOX_ROOT", "relative/path", 1);
    assert(installDir() == "/tmp/tuxblox_test_home/.tuxblox");
    unsetenv("TUXBLOX_ROOT");

    // A directory is only an install if it looks like one.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_looks_like_install";
        fs::remove_all(dir);
        fs::create_directories(dir);
        assert(!looksLikeInstall(dir.string()));

        std::ofstream(dir / "TuxBloxLauncher").put('x');
        assert(looksLikeInstall(dir.string()));
        fs::remove(dir / "TuxBloxLauncher");

        fs::create_directories(dir / "compat");
        std::ofstream(dir / "compat" / "main").put('x');
        assert(looksLikeInstall(dir.string()));
        fs::remove_all(dir);
    }

    // The free-space check has to measure the filesystem being installed to,
    // which may not exist yet, so it measures the nearest folder that does.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_ancestor";
        fs::remove_all(dir);
        fs::create_directories(dir);
        assert(existingAncestor(dir.string()) == dir.string());
        assert(existingAncestor((dir / "not" / "there" / "yet").string()) == dir.string());
        fs::remove_all(dir);
        // Everything gone: falls back to the root, which always exists.
        assert(existingAncestor((dir / "not" / "there").string()) ==
               fs::temp_directory_path().string());
        assert(existingAncestor("/definitely/not/here") == "/");
    }

    // selfExePath() is the installer's own path, not argv[0].
    {
        const std::string self = selfExePath();
        assert(!self.empty() && self[0] == '/');
        assert(self.find("test_install_paths") != std::string::npos);
    }

    unsetenv("HOME");
    bool threw = false;
    try {
        installDir();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);
    setenv("HOME", "/tmp/tuxblox_test_home", 1); // restore for anything running after

    assert(hasEnoughDiskSpace("/tmp", 1) == true);
    assert(hasEnoughDiskSpace("/tmp", (uint64_t)1 << 60) == false);

    printf("install_paths: all tests passed\n");
    return 0;
}
