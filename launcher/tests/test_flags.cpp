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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

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

    // Read before anything is initialised: the compiled-in defaults answer. Some code
    // will read a flag earlier than anyone planned, and it must not see an empty table.
    {
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2026-10-16");
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
        initFlags(rootWith("tuxblox_test_flags_norelease", nullptr), "stable", false, 5,
                  [&](const std::string&, int) { asked = true; return std::string(); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(!asked);
    }

    // With a release, the server is asked -- for the channel it was given.
    {
        std::string seen;
        initFlags(rootWith("tuxblox_test_flags_url", nullptr), "canary", true, 5,
                  [&](const std::string& url, int) { seen = url; return std::string("{}"); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(seen == clientFlagsUrl("canary", "x86_64"));
    }

    // The server's answer sits over the defaults.
    {
        initFlags(rootWith("tuxblox_test_flags_server", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxStringPrivacyPolicyVersion":"2030-01-01"})");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2030-01-01");
        // A flag the answer did not mention keeps its compiled-in value.
        assert(flagString("tbxStringTermsOfUseVersion") == "2026-10-17");
    }

    // The server may not set a local-only flag, whatever it sends.
    {
        initFlags(rootWith("tuxblox_test_flags_localonly", nullptr), "stable", true, 5,
                  [](const std::string&, int) {
                      return std::string(R"({"tbxFlagDisableUpdater":true})");
                  });
        assert(flagBool("tbxFlagDisableUpdater") == false);
    }

    // flags.json wins over the server, per key.
    {
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
        initFlags(rootWith("tuxblox_test_flags_localfile", R"({"tbxFlagDisableUpdater":true})"),
                  "stable", true, 5, [](const std::string&, int) { return std::string("{}"); });
        assert(flagBool("tbxFlagDisableUpdater") == true);
    }

    // A malformed flags.json is ignored WHOLE, not half-applied.
    {
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
        initFlags(rootWith("tuxblox_test_flags_arrayfile", "[1,2,3]"), "stable", true, 5,
                  [](const std::string&, int) { return std::string("{}"); });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2026-10-16");
    }

    // A fetch that fails leaves the lower sources answering, and does not throw.
    {
        initFlags(rootWith("tuxblox_test_flags_fetchfail", nullptr), "stable", true, 5,
                  [](const std::string&, int) -> std::string {
                      throw std::runtime_error("no route to host");
                  });
        assert(flagString("tbxStringPrivacyPolicyVersion") == "2026-10-16");
    }

    // A value does not change between two reads in one run.
    {
        int calls = 0;
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
        assert(flagChannelFor(settings, release) == "canary");

        settings.channel.clear();
        assert(flagChannelFor(settings, release) == "stable");

        release.present = false;
        assert(flagChannelFor(settings, release) == "stable");   // the safe default

        settings.channel = "experimental";
        assert(flagChannelFor(settings, release) == "experimental");

        // A release that is present but names no channel is not an answer.
        settings.channel.clear();
        release.present = true;
        release.file.channel.clear();
        assert(flagChannelFor(settings, release) == "stable");
    }

    printf("flags: all tests passed\n");
    return 0;
}
