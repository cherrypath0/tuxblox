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


#include "root_guard.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;

    // Absent: argv is untouched.
    {
        char a0[] = "TuxBloxLauncher";
        char a1[] = "--launch-studio";
        char* argv[] = {a0, a1, nullptr};
        int argc = 2;
        assert(!takeAllowRootFlag(argc, argv));
        assert(argc == 2);
        assert(std::string(argv[1]) == "--launch-studio");
    }

    // Present and first: removed, and the positional argument moves up into
    // argv[1] where the launcher's dispatch expects to find it.
    {
        char a0[] = "TuxBloxLauncher";
        char a1[] = "--allow-root";
        char a2[] = "roblox://placeId=1";
        char* argv[] = {a0, a1, a2, nullptr};
        int argc = 3;
        assert(takeAllowRootFlag(argc, argv));
        assert(argc == 2);
        assert(std::string(argv[1]) == "roblox://placeId=1");
        assert(argv[2] == nullptr);
    }

    // Present and last: the URI stays where it was.
    {
        char a0[] = "TuxBloxLauncher";
        char a1[] = "roblox://placeId=1";
        char a2[] = "--allow-root";
        char* argv[] = {a0, a1, a2, nullptr};
        int argc = 3;
        assert(takeAllowRootFlag(argc, argv));
        assert(argc == 2);
        assert(std::string(argv[1]) == "roblox://placeId=1");
        assert(argv[2] == nullptr);
    }

    // Given twice, both go.
    {
        char a0[] = "TuxBloxLauncher";
        char a1[] = "--allow-root";
        char a2[] = "--allow-root";
        char a3[] = "--launch-player";
        char* argv[] = {a0, a1, a2, a3, nullptr};
        int argc = 4;
        assert(takeAllowRootFlag(argc, argv));
        assert(argc == 2);
        assert(std::string(argv[1]) == "--launch-player");
    }

    // An ordinary user is never asked about any of this.
    assert(rootRunAllowed(false, 1000));
    assert(rootRunAllowed(true, 1000));

    // Root needs the flag.
    assert(!rootRunAllowed(false, 0));
    assert(rootRunAllowed(true, 0));

    // The decision is remembered for the exec sites that read it later.
    setAllowRoot(true);
    assert(allowRoot());
    setAllowRoot(false);
    assert(!allowRoot());

    printf("root_guard: all tests passed\n");
    return 0;
}
