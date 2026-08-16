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
        assert(s.channel == "stable");
        assert(s.autoUpdate == false);
    }

    // Round-trip, including sendCrashReports = false and a non-default channel.
    {
        Settings s;
        s.protonEnvVars = "PROTON_LOG=1 DXVK_HUD=fps";
        s.globalEnvVars = "MY_VAR=hello";
        s.sendCrashReports = false;
        s.channel = "canary";
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.protonEnvVars == "PROTON_LOG=1 DXVK_HUD=fps");
        assert(loaded.globalEnvVars == "MY_VAR=hello");
        assert(loaded.sendCrashReports == false);
        assert(loaded.channel == "canary");
    }

    // Malformed JSON -> defaults, not a crash.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << "{ not json";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars.empty());
        assert(s.globalEnvVars.empty());
        assert(s.sendCrashReports == true);
        assert(s.channel == "stable");
    }

    // Missing field -> defaults wholesale (loadSettings never throws, never
    // partially applies).
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "FOO=bar"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars.empty());
        assert(s.globalEnvVars.empty());
        assert(s.sendCrashReports == true);
        assert(s.channel == "stable");
    }

    // A settings.json written before "channel" existed (all three original
    // fields present, no channel key at all) must NOT wholesale-reset --
    // channel alone falls back to "stable" while everything else loads
    // normally. This is the one field read leniently (.value(), not .at())
    // specifically so upgrading the launcher doesn't wipe existing settings.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "FOO=bar", "global_env_vars": "BAZ=qux", "send_crash_reports": false})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars == "FOO=bar");
        assert(s.globalEnvVars == "BAZ=qux");
        assert(s.sendCrashReports == false);
        assert(s.channel == "stable");
    }

    // An unrecognized channel value (hand-edited or from a future version)
    // falls back to "stable" rather than being trusted verbatim.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "", "global_env_vars": "", "send_crash_reports": true, "channel": "nightly"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.channel == "stable");
    }

    // autoUpdate defaults to false, and (like channel) loads leniently --
    // a settings.json written before this field existed must not
    // wholesale-reset just because "auto_update" is missing.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "FOO=bar", "global_env_vars": "BAZ=qux", "send_crash_reports": false, "channel": "canary"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.protonEnvVars == "FOO=bar");
        assert(s.globalEnvVars == "BAZ=qux");
        assert(s.sendCrashReports == false);
        assert(s.channel == "canary");
        assert(s.autoUpdate == false);
    }

    // autoUpdate round-trips through save/load.
    {
        Settings s;
        s.autoUpdate = true;
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.autoUpdate == true);
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
