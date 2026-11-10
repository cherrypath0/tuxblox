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

#include "flags.h"
#include "flag_table.h"
#include "flag_startup.h"
#include "settings.h"
#include "release_file.h"
#include "launch_paths.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

// A root with a flags.json in it, or without one.
std::string rootWith(const char* name, const char* contents) {
    const fs::path dir = fs::temp_directory_path() / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    if (contents) std::ofstream(dir / "flags.json") << contents;
    return dir.string();
}

} // namespace

int main() {
    using namespace tuxblox;

    // The release is deliberately absent: one set per channel and architecture.
    {
        assert(clientFlagsUrl("canary", "x86_64") ==
               "https://config.tuxblox.net/v1/tuxblox/client/canary/x86_64/flags");
        assert(clientFlagsUrl("stable", "arm64") ==
               "https://config.tuxblox.net/v1/tuxblox/client/stable/arm64/flags");
    }

    // Pinned here rather than written as literals below: these change whenever the
    // policy does, and a test that breaks on a policy bump is a test nobody trusts.
    const std::string kCompiledInPolicy = flagString("tbxStringPrivacyPolicyVersion");
    const std::string kCompiledInTerms = flagString("tbxStringTermsOfUseVersion");

    // Read before anything is initialised: the compiled-in defaults answer. Some code
    // will read a flag earlier than anyone planned, and it must not see an empty table.
    {
        assert(!kCompiledInPolicy.empty());
        assert(!flagString("tbxStringPrivacyPolicyVersion").empty());
    }

    // A name no source defines reads the same as "defined and off". An offline install
    // whose defaults predate a flag cannot tell those two apart, so nor may a caller.
    {
        assert(flagBool("tbxFlagNobodyDefines") == false);
        assert(flagInt("tbxIntNobodyDefines") == 0);
        assert(flagString("tbxStringNobodyDefines").empty());
    }

    // No recorded release means no request at all -- that is every local run out of build/.
    {
        bool asked = false;
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_norelease", nullptr), "stable", false, 5,
                  [&](const std::string&, int) { asked = true; return std::string(); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(!asked);
    }

    // With a release, the server is asked -- for the channel it was given.
    {
        std::string seen;
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_url", nullptr), "canary", true, 5,
                  [&](const std::string& url, int) { seen = url; return std::string("{}"); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(seen == clientFlagsUrl("canary", "x86_64"));
    }

    // The server's answer sits over the defaults.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_server", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2030-01-01");
        // A flag the answer did not mention keeps its compiled-in value.
        assert(flagString("tbxStringTermsOfUseVersion") == kCompiledInTerms);
    }

    // The server may not set a local-only flag, whatever it sends.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_localonly", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxFlagDisableUpdater":true})");
                  });
        assert(flagBool("tbxFlagDisableUpdater") == false);
    }

    // flags.json wins over the server, per key.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_override",
                           R"({"tbxStringPrivacyPolicyVersion":"2040-01-01"})"),
                  "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01",)"
                                         R"("tbxStringTermsOfUseVersion":"2031-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2040-01-01");  // the file
        assert(flagString("tbxStringTermsOfUseVersion") == "2031-01-01");     // the server
    }

    // And flags.json MAY set a local-only flag -- that is the whole point of it.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_localfile", R"({"tbxFlagDisableUpdater":true})"),
                  "stable", true, 5, [](const std::string&, int) { return std::string("{}"); });
        assert(flagBool("tbxFlagDisableUpdater") == true);
    }

    // A malformed flags.json is ignored WHOLE, not half-applied.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_badfile",
                           R"({"tbxStringPrivacyPolicyVersion":"2040-01-01",)"),
                  "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2030-01-01");
    }

    // A flags.json that is not an object changes nothing.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_arrayfile", "[1,2,3]"), "stable", true, 5,
                  [](const std::string&, int) { return std::string("{}"); });
        assert(!flagString("tbxStringPrivacyPolicyVersion").empty());
    }

    // A fetch that fails leaves the lower sources answering, and does not throw.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_fetchfail", nullptr), "stable", true, 5,
                  [](const std::string&, int) -> std::string {
                      throw std::runtime_error("no route to host");
                  });
        assert(!flagString("tbxStringPrivacyPolicyVersion").empty());
    }

    // A value does not change between two reads in one run.
    {
        int calls = 0;
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_sealed", nullptr), "stable", true, 5,
                  [&](const std::string&, int) {
                      calls++;
                      return std::string(R"({"tbxStringTermsOfUseVersion":"2031-01-01"})");
                  });
        const std::string first = flagString("tbxStringTermsOfUseVersion");
        const std::string second = flagString("tbxStringTermsOfUseVersion");
        assert(first == second);
        assert(calls == 1);   // sealed: the second read asks nobody
    }

    // The timeout the caller asked for is what the fetch is given.
    {
        int budget = 0;
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_budget", nullptr), "stable", true, 2,
                  [&](const std::string&, int seconds) { budget = seconds; return std::string("{}"); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(budget == 2);
    }

    // Which channel keys the request. The one the user follows wins, because switching
    // channel means switching everywhere else in the launcher too; the installed
    // release's channel answers only when settings say nothing.
    {
        Settings settings;
        RecordedRelease release;

        settings.channel = "canary";
        release.present = true;
        release.file.channel = "stable";
        assert(flagChannelFor(settings, true, release) == "canary");

        // No settings.json: the release's channel answers. loadSettings() defaults the
        // channel to "stable", so without the presence flag a fresh canary install
        // would read stable's flags and nothing would say why.
        assert(flagChannelFor(settings, false, release) == "stable");
        release.file.channel = "canary";
        assert(flagChannelFor(settings, false, release) == "canary");

        // Neither says anything: the same default loadSettings() uses, so the flags
        // channel cannot disagree with the one the updater follows.
        release.present = false;
        settings.channel = "stable";
        assert(flagChannelFor(settings, false, release) == "stable");

        settings.channel = "experimental";
        assert(flagChannelFor(settings, true, release) == "experimental");

        // A release that is present but names no channel is not an answer.
        release.present = true;
        release.file.channel.clear();
        assert(flagChannelFor(settings, false, release) == "experimental");
    }

    // What the launcher hands the layer. The layer has no settings and no release file
    // of its own, so without these two it cannot key a request -- which is exactly what
    // should happen for a build that is not a release.
    {
        const fs::path dir = fs::temp_directory_path() / "tuxblox_test_launch_env";
        fs::remove_all(dir);
        fs::create_directories(dir);

        auto has = [](const std::vector<std::string>& env, const std::string& prefix) {
            for (const std::string& entry : env) {
                if (entry.rfind(prefix, 0) == 0) return true;
            }
            return false;
        };

        // No recorded release: a channel is still named, but nothing says it is released,
        // so the layer asks nobody.
        std::vector<std::string> env = flagEnvVars(dir.string());
        assert(has(env, "TUXBLOX_FLAGS_CHANNEL="));
        assert(!has(env, "TUXBLOX_FLAGS_RELEASED="));

        std::ofstream(dir / "release.json")
            << R"({"release": "3.6.0.0", "channel": "canary", "changelog": "c", )"
               R"("source_commit": "abc123", "source_dirty": false, "components": {)"
               R"("launcher": {"version": "3.5.0", "arch": {"x86_64": {"sha256": "aa", "size": 12}, "arm64": null}}}})";
        env = flagEnvVars(dir.string());
        assert(has(env, "TUXBLOX_FLAGS_CHANNEL=canary"));
        assert(has(env, "TUXBLOX_FLAGS_RELEASED=1"));
        fs::remove_all(dir);
    }

    // A second initFlags after the table is sealed must NOT change a value. The whole
    // point of sealing is that a feature reading its flag twice cannot get two answers,
    // and some code always reads a flag earlier than anyone planned.
    {
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_reinit", nullptr), "stable", true, 5,
                  [](const std::string&, int) { return std::string("{}"); });
        const std::string before = flagString("tbxStringPrivacyPolicyVersion");

        initFlags(rootWith("tuxblox_test_flags_reinit2", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == before);

        // Asking explicitly is still allowed -- that is a decision, not an accident.
        resetFlags();
        initFlags(rootWith("tuxblox_test_flags_reinit3", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2030-01-01");
        resetFlags();
    }

    printf("flags: all tests passed\n");
    return 0;
}
