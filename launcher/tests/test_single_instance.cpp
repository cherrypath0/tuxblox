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

#include "single_instance.h"
#include <cassert>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // flock() locks belong to the open file description, not the process,
    // so acquiring it twice in this same test process still exercises real
    // mutual exclusion -- the second call opens a distinct file description
    // on the same lock file and must fail to lock it, exactly like a second
    // launcher process would.
    fs::path dir = fs::temp_directory_path() / "tuxblox_test_single_instance";
    fs::remove_all(dir);
    fs::create_directories(dir);

    assert(acquireSingleInstanceLock(dir.string()) == true);
    assert(acquireSingleInstanceLock(dir.string()) == false);

    fs::remove_all(dir);

    // installDir doesn't exist and can't be created (no such parent) --
    // fails open rather than blocking every launch on a lock file it can't
    // manage.
    assert(acquireSingleInstanceLock("/tuxblox_test_single_instance_no_such_parent/sub") == true);

    printf("single_instance: all tests passed\n");
    return 0;
}
