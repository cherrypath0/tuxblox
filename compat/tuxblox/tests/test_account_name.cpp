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
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // THE CANONICAL TABLE. Wine's copy in dlls/advapi32/advapi.c answers the
    // same way for every row; changing one without the other is the bug this
    // list exists to catch.
    struct Row { const char* raw; const char* want; };
    const Row rows[] = {
        {"cherry",                 "cherry"},
        {"root",                   "root"},
        {"Some User",              "Some User"},
        {"a",                      "a"},
        {"12345678901234567890",   "12345678901234567890"},   // 20, the Windows limit
        {"123456789012345678901",  "user"},                   // 21, one too many
        {"",                       "user"},
        {".",                      "user"},
        {"..",                     "user"},
        {"has/slash",              "user"},
        {"has\\backslash",         "user"},
        {"has:colon",              "user"},
        {"has*star",               "user"},
        {"has?question",           "user"},
        {"has\"quote",             "user"},
        {"has<less",               "user"},
        {"has>greater",            "user"},
        {"has|pipe",               "user"},
        {"has\tcontrol",           "user"},
        {"trailing.",              "user"},
        {"trailing ",              "user"},
        {"NUL",                    "user"},
        {"nul",                    "user"},
        {"con",                    "user"},
        {"COM1",                   "user"},
        {"lpt9",                   "user"},
        {"Public",                 "user"},                   // the drive's shared folder
        {"public",                 "user"},
        {"COM10",                  "COM10"},                  // not a device name
        {"console",                "console"},                // only the exact names
    };
    for (const Row& row : rows) {
        const std::string got = sanitizedAccountName(row.raw);
        if (got != row.want) {
            printf("sanitizedAccountName(\"%s\") = \"%s\", wanted \"%s\"\n",
                   row.raw, got.c_str(), row.want);
            return 1;
        }
    }

    // Whatever the host account is called, the answer is usable as a folder.
    {
        const std::string name = accountName();
        assert(!name.empty());
        assert(name.find('/') == std::string::npos);
        assert(sanitizedAccountName(name) == name);
    }

    // The folder that is actually in the drive is what gets reported, because
    // the compatibility layer follows what Wine decided rather than guessing.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_account_name";
        fs::remove_all(base);

        // Nothing there at all.
        assert(prefixAccountName(base, "cherry") == "user");

        // Only Public: still nothing to report.
        fs::create_directories(base / "drive_c/users/Public");
        assert(prefixAccountName(base, "cherry") == "user");

        // The account folder beside it is the answer.
        fs::create_directories(base / "drive_c/users/cherry");
        assert(prefixAccountName(base, "cherry") == "cherry");

        // Two of them: the one matching the host account is the one Wine will
        // use, because that is what %USERPROFILE% expands to. Picking "user"
        // here would be the single answer guaranteed to disagree with it.
        fs::create_directories(base / "drive_c/users/user");
        assert(prefixAccountName(base, "cherry") == "cherry");
        assert(prefixAccountName(base, "user") == "user");

        // Neither matches: genuinely ambiguous, so say nothing and let the
        // caller decide rather than guessing between two strangers.
        assert(prefixAccountName(base, "someone-else") == "");

        fs::remove_all(base);
    }

    printf("account_name: all tests passed\n");
    return 0;
}
