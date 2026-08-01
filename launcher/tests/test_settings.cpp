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

#include "settings.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    const std::string dir = (fs::temp_directory_path() / "tuxblox_test_settings_dir").string();
    fs::remove_all(dir);

    // Missing file -> defaults.
    {
        Settings s = loadSettings(dir);
        assert(s.protonEnvVars.empty());
        assert(s.globalEnvVars.empty());
        assert(s.sendCrashReports == true);
    }

    // Round-trip, including sendCrashReports = false.
    {
        Settings s;
        s.protonEnvVars = "PROTON_LOG=1 DXVK_HUD=fps";
        s.globalEnvVars = "MY_VAR=hello";
        s.sendCrashReports = false;
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.protonEnvVars == "PROTON_LOG=1 DXVK_HUD=fps");
        assert(loaded.globalEnvVars == "MY_VAR=hello");
        assert(loaded.sendCrashReports == false);
    }

    // Malformed JSON -> defaults, not a crash.
    {
        std::ofstream out(dir + "/launcher_settings.json", std::ios::binary);
        out << "{ not json";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars.empty());
        assert(s.globalEnvVars.empty());
        assert(s.sendCrashReports == true);
    }

    // Missing field -> defaults wholesale (loadSettings never throws, never
    // partially applies).
    {
        std::ofstream out(dir + "/launcher_settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "FOO=bar"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars.empty());
        assert(s.globalEnvVars.empty());
        assert(s.sendCrashReports == true);
    }

    // parseEnvPairs: empty string.
    {
        auto pairs = parseEnvPairs("");
        assert(pairs.empty());
    }

    // parseEnvPairs: multiple pairs, extra/repeated whitespace tolerated.
    {
        auto pairs = parseEnvPairs("  FOO=bar   BAZ=qux  ");
        assert(pairs.size() == 2);
        assert(pairs[0] == "FOO=bar");
        assert(pairs[1] == "BAZ=qux");
    }

    // parseEnvPairs: a token with no '=' is skipped.
    {
        auto pairs = parseEnvPairs("FOO=bar NOTAPAIR BAZ=qux");
        assert(pairs.size() == 2);
        assert(pairs[0] == "FOO=bar");
        assert(pairs[1] == "BAZ=qux");
    }

    fs::remove_all(dir);

    printf("settings: all tests passed\n");
    return 0;
}
