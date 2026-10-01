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

    // The binary's own directory is the install root: this is what lets a
    // manual install at /opt/tuxblox work without being told where it is.
    {
        unsetenv("TUXBLOX_ROOT");
        const std::string self = selfExePath();
        const std::string expected = self.substr(0, self.rfind('/'));
        assert(installDir() == expected);
    }

    // TUXBLOX_ROOT overrides it, with any trailing slash dropped so the
    // paths built from it never contain a doubled separator.
    setenv("TUXBLOX_ROOT", "/opt/tuxblox", 1);
    assert(installDir() == "/opt/tuxblox");
    setenv("TUXBLOX_ROOT", "/opt/tuxblox/", 1);
    assert(installDir() == "/opt/tuxblox");
    setenv("TUXBLOX_ROOT", "/opt/tuxblox///", 1);
    assert(installDir() == "/opt/tuxblox");

    // A relative value is ignored rather than resolved against whatever the
    // current directory happens to be.
    {
        setenv("TUXBLOX_ROOT", "tuxblox", 1);
        const std::string self = selfExePath();
        assert(installDir() == self.substr(0, self.rfind('/')));
        unsetenv("TUXBLOX_ROOT");
    }

    // HOME is no longer consulted when /proc/self/exe is readable, so an
    // unset HOME is not an error any more.
    unsetenv("HOME");
    {
        const std::string self = selfExePath();
        assert(installDir() == self.substr(0, self.rfind('/')));
    }
    setenv("HOME", "/tmp/tuxblox_test_home", 1); // restore for anything running after

    assert(hasEnoughDiskSpace("/tmp", 1) == true);
    assert(hasEnoughDiskSpace("/tmp", (uint64_t)1 << 60) == false);

    // Neither directory present: the current name is what gets reported.
    assert(compatDirUnder("/x/tuxblox") == "/x/tuxblox/compat");

    // An install still carrying the old name is found under it, and the
    // current name wins whenever both are there.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_legacy";
        fs::remove_all(dir);
        fs::create_directories(dir / "proton");
        assert(compatDirUnder(dir.string()) == (dir / "proton").string());
        fs::create_directories(dir / "compat");
        assert(compatDirUnder(dir.string()) == (dir / "compat").string());
        fs::remove_all(dir);
    }

    // Missing proton binary -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_missing";
        fs::remove_all(dir);
        auto v = readInstalledCompatVersion(dir.string());
        assert(!v.has_value());
    }

    // Working main (faked with a script) -> first line of its --version output.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_ok";
        fs::remove_all(dir);
        fs::create_directories(dir / "compat");
        {
            std::ofstream out(dir / "compat" / "main");
            out << "#!/bin/sh\necho 0.1.0\n";
        }
        fs::permissions(dir / "compat" / "main", fs::perms::owner_all);
        auto v = readInstalledCompatVersion(dir.string());
        assert(v.has_value() && *v == "0.1.0");
        fs::remove_all(dir);
    }

    // Binary that exits nonzero -> nullopt.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_install_paths_failing";
        fs::remove_all(dir);
        fs::create_directories(dir / "compat");
        {
            std::ofstream out(dir / "compat" / "main");
            out << "#!/bin/sh\nexit 1\n";
        }
        fs::permissions(dir / "compat" / "main", fs::perms::owner_all);
        auto v = readInstalledCompatVersion(dir.string());
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

    // The interface libraries are the libtuxblox folder beside the running binary
    {
        const std::string root = tuxblox::interfaceStackRoot();
        const std::string self = tuxblox::selfExePath();
        assert(root == self.substr(0, self.rfind('/')) + "/libtuxblox");
    }

    printf("install_paths: all tests passed\n");
    return 0;
}
