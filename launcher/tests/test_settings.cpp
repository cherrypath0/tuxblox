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
        assert(s.envVars.empty());
        assert(s.sendCrashReports == true);
        assert(s.channel == "stable");
        assert(s.autoUpdate == false);
    }

    // Round-trip, including sendCrashReports = false and a non-default channel.
    {
        Settings s;
        s.envVars = "TUXBLOX_LOG=1 DXVK_HUD=fps MY_VAR=hello";
        s.sendCrashReports = false;
        s.channel = "canary";
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.envVars == "TUXBLOX_LOG=1 DXVK_HUD=fps MY_VAR=hello");
        assert(loaded.sendCrashReports == false);
        assert(loaded.channel == "canary");
    }

    // Malformed JSON -> defaults, not a crash.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << "{ not json";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.envVars.empty());
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
        assert(s.envVars.empty());
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
        assert(s.envVars == "FOO=bar BAZ=qux");
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
        assert(s.envVars == "FOO=bar BAZ=qux");
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

    // gpu defaults to "" (let the system decide) and round-trips as a PCI slot.
    {
        Settings s;
        assert(s.gpu.empty());
        s.gpu = "0000:01:00.0";
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.gpu == "0000:01:00.0");
    }

    // gpu loads leniently, like channel and auto_update: a settings.json
    // written before the graphics-card picker existed must keep everything
    // else it holds rather than resetting to defaults.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"env_vars": "FOO=bar", "send_crash_reports": false, "channel": "canary", "auto_update": true})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.gpu.empty());
        assert(s.envVars == "FOO=bar");
        assert(s.sendCrashReports == false);
        assert(s.channel == "canary");
        assert(s.autoUpdate == true);
    }

    // launchEnvPairs(): with the Automatic default, a launch carries exactly
    // the user's own variables and nothing else. This is the guarantee that
    // the picker changes nothing for anyone who never touches it, so it is
    // asserted rather than assumed.
    {
        Settings s;
        s.envVars = "FOO=bar BAZ=qux";
        assert(s.gpu.empty());

        auto pairs = launchEnvPairs(s);
        assert(pairs.size() == 2);
        assert(pairs[0] == "FOO=bar");
        assert(pairs[1] == "BAZ=qux");
    }

    // launchEnvPairs(): a saved card that this machine does not have falls
    // back to automatic, rather than steering rendering onto a card that
    // isn't there. (A real PCI slot cannot be asserted here -- the test
    // machine's own hardware is whatever it is -- but a plainly bogus slot
    // must never match.)
    {
        Settings s;
        s.gpu = "ffff:ff:ff.f";
        s.envVars = "FOO=bar";

        auto pairs = launchEnvPairs(s);
        assert(pairs.size() == 1);
        assert(pairs[0] == "FOO=bar");
    }

    // launchEnvPairs(): the user's variables come last, which is what makes
    // an entry in the settings box override the same variable from the
    // picker. Order is the contract here, so it is checked directly.
    {
        Settings s;
        s.envVars = "DRI_PRIME=9";
        auto pairs = launchEnvPairs(s);
        assert(!pairs.empty());
        assert(pairs.back() == "DRI_PRIME=9");
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

    fs::create_directories(dir);
    // FastFlags round-trip, keeping their order and their per-target split.
    {
        Settings s;
        s.fastFlags.player = {{"DFIntTaskSchedulerTargetFps", "60"}, {"FFlagA", "True"}};
        s.fastFlags.studio = {{"FFlagStudioOnly", "False"}};
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.fastFlags.player.size() == 2);
        assert(loaded.fastFlags.player[0].name == "DFIntTaskSchedulerTargetFps");
        assert(loaded.fastFlags.player[0].value == "60");
        assert(loaded.fastFlags.player[1].name == "FFlagA");
        assert(loaded.fastFlags.studio.size() == 1);
        assert(loaded.fastFlags.studio[0].name == "FFlagStudioOnly");
    }

    // A settings.json written before FastFlags existed keeps everything else
    // rather than resetting, same lenient rule as channel and auto_update.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"env_vars": "FOO=bar", "send_crash_reports": false, "channel": "canary"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.envVars == "FOO=bar");
        assert(s.channel == "canary");
        assert(s.sendCrashReports == false);
        assert(s.fastFlags.player.empty());
        assert(s.fastFlags.studio.empty());
    }

    // A malformed fast_flags block costs the flags, never the other settings:
    // wrong type for the block, wrong type for an entry, and a nameless entry.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"env_vars": "FOO=bar", "send_crash_reports": true, "fast_flags": "nonsense"})";
        out.close();
        Settings s = loadSettings(dir);
        assert(s.envVars == "FOO=bar");
        assert(s.fastFlags.player.empty());

        std::ofstream out2(dir + "/settings.json", std::ios::binary);
        out2 << R"({"send_crash_reports": true, "fast_flags": {"player": [3, {"value": "novalue"},
                    {"name": "Good", "value": "1"}]}})";
        out2.close();
        Settings s2 = loadSettings(dir);
        assert(s2.fastFlags.player.size() == 1);
        assert(s2.fastFlags.player[0].name == "Good");
    }

    // settings.json from before the two env fields were merged: the old
    // Proton and Global values are joined into the single field rather than
    // one of them being dropped.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "DXVK_HUD=fps", "global_env_vars": "MY_VAR=hello", "send_crash_reports": true})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.envVars == "DXVK_HUD=fps MY_VAR=hello");
    }

    // Only one of the two legacy fields set -- no stray separator either side.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"proton_env_vars": "", "global_env_vars": "ONLY=global", "send_crash_reports": true})";
        out.close();
        assert(loadSettings(dir).envVars == "ONLY=global");

        std::ofstream out2(dir + "/settings.json", std::ios::binary);
        out2 << R"({"proton_env_vars": "ONLY=proton", "global_env_vars": "", "send_crash_reports": true})";
        out2.close();
        assert(loadSettings(dir).envVars == "ONLY=proton");
    }

    // The new field wins outright when present -- a file already migrated
    // must not have the stale legacy keys appended a second time.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"env_vars": "NEW=1", "proton_env_vars": "OLD=2", "global_env_vars": "OLD=3", "send_crash_reports": true})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.envVars == "NEW=1");
    }

    // haptics defaults to on. Controller vibration is what a user expects a
    // controller to do, so the default is the working one.
    {
        Settings s;
        assert(s.haptics == true);
    }

    // haptics loads leniently: a settings.json written before this field
    // existed keeps vibration on rather than silently losing it.
    {
        std::ofstream out(dir + "/settings.json", std::ios::binary);
        out << R"({"env_vars": "FOO=bar", "send_crash_reports": false, "channel": "canary"})";
        out.close();

        Settings s = loadSettings(dir);
        assert(s.haptics == true);
    }

    // haptics round-trips through save/load, off included.
    {
        Settings s;
        s.haptics = false;
        saveSettings(dir, s);

        Settings loaded = loadSettings(dir);
        assert(loaded.haptics == false);
    }

    // launchEnvPairs(): haptics on is the default, and a default launch must
    // still carry nothing but the user's own variables -- same guarantee the
    // graphics-card picker keeps.
    {
        Settings s;
        s.envVars = "FOO=bar";
        assert(s.haptics == true);

        auto pairs = launchEnvPairs(s);
        assert(pairs.size() == 1);
        assert(pairs[0] == "FOO=bar");
    }

    // launchEnvPairs(): turning haptics off is what emits anything at all.
    // The compatibility layer reads this and routes the pad down the path
    // that has no vibration.
    {
        Settings s;
        s.haptics = false;

        auto pairs = launchEnvPairs(s);
        assert(pairs.size() == 1);
        assert(pairs[0] == "TUXBLOX_HAPTICS=0");
    }

    // A user variable still wins over the haptics pair, because it is
    // appended after it.
    {
        Settings s;
        s.haptics = false;
        s.envVars = "TUXBLOX_HAPTICS=1";

        auto pairs = launchEnvPairs(s);
        assert(pairs.size() == 2);
        assert(pairs[0] == "TUXBLOX_HAPTICS=0");
        assert(pairs[1] == "TUXBLOX_HAPTICS=1");
    }

    fs::remove_all(dir);

    printf("settings: all tests passed\n");
    return 0;
}
