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
#include <stdexcept>
#include <string>

int main() {
    using tuxblox::installDir;
    using tuxblox::hasEnoughDiskSpace;

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

    printf("install_paths: all tests passed\n");
    return 0;
}
