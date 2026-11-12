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

#include "prefix_user.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    const fs::path base = fs::temp_directory_path() / "tuxblox_test_prefix_user";
    fs::remove_all(base);

    assert(driveCDir("/opt/tuxblox") == "/opt/tuxblox/runtime/pfx/drive_c");

    // No drive yet: the old name is the safe answer, since that is what a
    // virtual drive built before this change is called.
    assert(prefixUserName(driveCDir(base.string())) == "user");
    assert(prefixUserDir(base.string()) ==
           (base / "runtime/pfx/drive_c/users/user").string());

    // Public is never the account folder.
    fs::create_directories(base / "runtime/pfx/drive_c/users/Public");
    assert(prefixUserName(driveCDir(base.string())) == "user");

    // The real one is found beside it.
    fs::create_directories(base / "runtime/pfx/drive_c/users/cherry");
    assert(prefixUserName(driveCDir(base.string())) == "cherry");
    assert(prefixUserDir(base.string()) ==
           (base / "runtime/pfx/drive_c/users/cherry").string());

    // Two account folders: the one matching the host account is the one Wine
    // names in %USERPROFILE%, so that is the one to follow. Answering "user"
    // here would be the single choice guaranteed to disagree with it.
    fs::create_directories(base / "runtime/pfx/drive_c/users/user");
    assert(prefixUserName(driveCDir(base.string()), "cherry") == "cherry");
    assert(prefixUserName(driveCDir(base.string()), "user") == "user");
    // Neither is the host's: nothing better to say than the old name.
    assert(prefixUserName(driveCDir(base.string()), "someone-else") == "user");

    // The host account is read for callers that do not name one.
    assert(!hostAccountName().empty());

    // Where Roblox's data is. With nothing pointing anywhere yet it is the folder
    // inside the drive, which is where a first install puts things.
    {
        const fs::path account = base / "runtime/pfx/drive_c/users/cherry";
        const fs::path inDrive = account / "AppData/Local/Roblox";
        assert(robloxDataDirIn(account.string()) == inDrive.string());

        // Pointed somewhere: that is what the compatibility layer chose, so it is the
        // answer. Every program reads it back the same way rather than working it out
        // again, which is what stops two of them disagreeing.
        const fs::path kept = base / "elsewhere/roblox";
        fs::create_directories(kept);
        fs::create_directories(inDrive.parent_path());
        fs::create_directory_symlink(kept, inDrive);
        assert(robloxDataDirIn(account.string()) == kept.string());
        assert(robloxDataDir(base.string()) == kept.string());

        // A relative target is read against a directory this is not standing in, so it
        // is not followed -- the folder inside the drive answers instead and the file
        // system resolves the link itself, as it did before any of this.
        fs::remove(inDrive);
        fs::create_directory_symlink("../../../../../elsewhere/roblox", inDrive);
        assert(robloxDataDirIn(account.string()) == inDrive.string());
        fs::remove(inDrive);
    }

    fs::remove_all(base);
    printf("prefix_user: all tests passed\n");
    return 0;
}
