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
    using namespace tuxblox;

    setenv("HOME", "/tmp/tuxblox_test_home", 1);
    assert(installDir() == "/tmp/tuxblox_test_home/.tuxblox");

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

    assert(protonDirUnder("/x/tuxblox") == "/x/tuxblox/proton");

    // Missing proton binary -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_missing";
        fs::remove_all(dir);
        auto v = readInstalledProtonVersion(dir.string());
        assert(!v.has_value());
    }

    // Working main (faked with a script) -> first line of its --version output.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_ok";
        fs::remove_all(dir);
        fs::create_directories(dir / "proton");
        {
            std::ofstream out(dir / "proton" / "main");
            out << "#!/bin/sh\necho 0.1.0\n";
        }
        fs::permissions(dir / "proton" / "main", fs::perms::owner_all);
        auto v = readInstalledProtonVersion(dir.string());
        assert(v.has_value() && *v == "0.1.0");
        fs::remove_all(dir);
    }

    // Binary that exits nonzero -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_failing";
        fs::remove_all(dir);
        fs::create_directories(dir / "proton");
        {
            std::ofstream out(dir / "proton" / "main");
            out << "#!/bin/sh\nexit 1\n";
        }
        fs::permissions(dir / "proton" / "main", fs::perms::owner_all);
        auto v = readInstalledProtonVersion(dir.string());
        assert(!v.has_value());
        fs::remove_all(dir);
    }

    // selfExePath() moved here from main.cpp so watch_launch.cpp can reuse it
    // (it needs the launcher's own path to write shortcut Exec= lines).
    {
        const std::string self = selfExePath();
        assert(!self.empty());
        assert(self[0] == '/');
        assert(self.find("test_install_paths") != std::string::npos);
    }

    printf("install_paths: all tests passed\n");
    return 0;
}
