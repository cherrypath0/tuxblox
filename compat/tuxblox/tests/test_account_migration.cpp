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

#include "../src/support/account_name.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& file) {
    std::ifstream in(file);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// A drive with the old "user" account folder and the registry that goes with it.
fs::path makeOldPrefix(const fs::path& base) {
    fs::remove_all(base);
    fs::create_directories(base / "drive_c/users/user/AppData/Local/Roblox");
    fs::create_directories(base / "drive_c/users/Public");
    std::ofstream(base / "user.reg")
        << "[Volatile Environment]\n\"USERPROFILE\"=\"C:\\\\users\\\\user\"\n";
    std::ofstream(base / "system.reg")
        << "[ProfileList]\n\"ProfileImagePath\"=\"C:\\\\users\\\\user\"\n";
    std::ofstream(base / "userdef.reg") << "[nothing]\n";
    return base;
}

} // namespace

int main() {
    using namespace tuxblox;

    const fs::path base = fs::temp_directory_path() / "tuxblox_test_account_migration";

    // The ordinary case: the folder moves and the drive's own settings follow
    // it, so the installed Roblox inside it is still where the drive says.
    {
        makeOldPrefix(base);
        assert(migrateAccountFolder(base, "cherry") == AccountFolderMigration::Renamed);
        assert(fs::exists(base / "drive_c/users/cherry/AppData/Local/Roblox"));
        assert(!fs::exists(base / "drive_c/users/user"));
        assert(readFile(base / "user.reg").find("C:\\\\users\\\\cherry") != std::string::npos);
        assert(readFile(base / "user.reg").find("C:\\\\users\\\\user") == std::string::npos);
        assert(readFile(base / "system.reg").find("C:\\\\users\\\\cherry") != std::string::npos);

        // Running again changes nothing: the folder's own name is the marker.
        assert(migrateAccountFolder(base, "cherry") == AccountFolderMigration::NotNeeded);
        assert(fs::exists(base / "drive_c/users/cherry/AppData/Local/Roblox"));
    }

    // A drive already on the real name, with no old folder, is left alone.
    {
        fs::remove_all(base);
        fs::create_directories(base / "drive_c/users/cherry");
        assert(migrateAccountFolder(base, "cherry") == AccountFolderMigration::NotNeeded);
    }

    // Both folders present: say so and change nothing, rather than picking one.
    {
        makeOldPrefix(base);
        fs::create_directories(base / "drive_c/users/cherry");
        assert(migrateAccountFolder(base, "cherry") == AccountFolderMigration::Ambiguous);
        assert(fs::exists(base / "drive_c/users/user"));
        assert(fs::exists(base / "drive_c/users/cherry"));
        assert(readFile(base / "user.reg").find("C:\\\\users\\\\user") != std::string::npos);
    }

    // The name we would rename to is the fallback: nothing to do.
    {
        makeOldPrefix(base);
        assert(migrateAccountFolder(base, "user") == AccountFolderMigration::NotNeeded);
        assert(fs::exists(base / "drive_c/users/user"));
    }

    // A registry file that cannot be written must not be reported as a clean
    // migration, and must not be left truncated -- a half-written system.reg
    // is worse than one that still names the old folder.
    {
        makeOldPrefix(base);
        const std::string before = readFile(base / "system.reg");
        fs::permissions(base / "system.reg", fs::perms::owner_read);

        const AccountFolderMigration result = migrateAccountFolder(base, "cherry");
        fs::permissions(base / "system.reg", fs::perms::owner_all);

        // Running as root defeats the permission bits, so only assert the
        // partial-failure path when the write really was refused.
        if (result == AccountFolderMigration::RegistryIncomplete) {
            assert(fs::exists(base / "drive_c/users/cherry"));
            // Untouched, not truncated.
            assert(readFile(base / "system.reg") == before);
            // The one that could be written did get written.
            assert(readFile(base / "user.reg").find("C:\\users\\cherry") != std::string::npos);
        } else {
            assert(result == AccountFolderMigration::Renamed);
        }
    }

    // A rename that cannot happen reports failure and leaves the drive usable
    // on the old name, because a failed migration must never fail a launch.
    {
        makeOldPrefix(base);
        fs::permissions(base / "drive_c/users", fs::perms::owner_read | fs::perms::owner_exec);
        const AccountFolderMigration result = migrateAccountFolder(base, "cherry");
        fs::permissions(base / "drive_c/users", fs::perms::owner_all);

        // Running as root defeats the permission bits, so only assert the
        // failure path when the rename really was refused.
        if (result == AccountFolderMigration::Failed) {
            assert(fs::exists(base / "drive_c/users/user/AppData/Local/Roblox"));
            assert(readFile(base / "user.reg").find("C:\\\\users\\\\user") != std::string::npos);
        } else {
            assert(result == AccountFolderMigration::Renamed);
        }
    }

    // The launcher works out the executable path before this process starts,
    // so on the very first launch after an upgrade it still names the old
    // account folder -- which this run is about to rename out from under it.
    {
        makeOldPrefix(base);
        const std::string stale =
            (base / "drive_c/users/user/AppData/Local/Roblox/Versions/version-a/RobloxStudioBeta.exe")
                .string();
        fs::create_directories(fs::path(stale).parent_path());
        std::ofstream(stale).put('M');

        assert(migrateAccountFolder(base, "cherry") == AccountFolderMigration::Renamed);
        assert(!fs::exists(stale));

        const std::string fixed = retargetAfterAccountRename(stale, base, "cherry");
        assert(fixed ==
               (base / "drive_c/users/cherry/AppData/Local/Roblox/Versions/version-a/RobloxStudioBeta.exe")
                   .string());
        assert(fs::exists(fixed));

        // A path that still resolves is never touched.
        assert(retargetAfterAccountRename(fixed, base, "cherry") == fixed);

        // Nor is anything that is not inside this drive's old account folder.
        assert(retargetAfterAccountRename("roblox-player://placeId=1", base, "cherry") ==
               "roblox-player://placeId=1");
        assert(retargetAfterAccountRename("/somewhere/else.exe", base, "cherry") ==
               "/somewhere/else.exe");

        // And a path whose replacement does not exist either is left alone,
        // so a genuinely missing file still reports itself as missing.
        const std::string gone =
            (base / "drive_c/users/user/AppData/Local/Roblox/Versions/version-z/None.exe").string();
        assert(retargetAfterAccountRename(gone, base, "cherry") == gone);
    }

    fs::remove_all(base);
    printf("account_migration: all tests passed\n");
    return 0;
}
