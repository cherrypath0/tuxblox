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

#include "roblox_deploy.h"
#include <cassert>
#include <cstdio>
#include <stdexcept>

int main() {
    using namespace tuxblox;

    // robloxBinaryType(): Roblox's own binaryType strings.
    assert(std::string(robloxBinaryType(LaunchTarget::Player)) == "WindowsPlayer");
    assert(std::string(robloxBinaryType(LaunchTarget::Studio)) == "WindowsStudio64");

    // parseClientVersionHash(): fixture modeling the publicly documented
    // clientsettings.roblox.com/v2/client-version/{binaryType} response
    // shape (field name "clientVersionUpload") -- NOT re-verified against a
    // live response in this repo, see this task's open-questions note.
    {
        const std::string fixture = R"({
          "version": "0.601.1.6060000",
          "clientVersionUpload": "version-c8cc98b524d34a0c",
          "bootstrapperVersion": "1.6.0.60600"
        })";
        assert(parseClientVersionHash(fixture) == "version-c8cc98b524d34a0c");
    }

    // parseClientVersionHash(): missing field -> throws rather than
    // returning something silently wrong.
    {
        bool threw = false;
        try {
            parseClientVersionHash(R"({"version": "0.601.1.6060000"})");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
    }

    // previousVersionFromDeployHistory(): fixture modeling the publicly
    // documented DeployHistory.txt line format ("New <BinaryType>
    // version-<hash> at <date>") -- NOT re-verified live, see open
    // questions. Entries assumed chronological (oldest first, as observed
    // in public samples); the function must not assume any particular
    // ordering beyond "find current, return the one textually before it".
    {
        const std::string fixture =
            "New WindowsPlayer version-aaa111 at 1/1/2026 10:00:00 AM\r\n"
            "New WindowsStudio64 version-bbb222 at 1/2/2026 10:00:00 AM\r\n"
            "New WindowsPlayer version-ccc333 at 1/3/2026 10:00:00 AM\r\n"
            "New WindowsPlayer version-ddd444 at 1/4/2026 10:00:00 AM\r\n";

        auto prev = previousVersionFromDeployHistory(fixture, "WindowsPlayer", "version-ddd444");
        assert(prev.has_value() && *prev == "version-ccc333");

        // Oldest entry for its binary type has no predecessor.
        auto none = previousVersionFromDeployHistory(fixture, "WindowsPlayer", "version-aaa111");
        assert(!none.has_value());

        // Hash not present at all -> nullopt, not a throw (a purged/unknown
        // hash is an expected, not exceptional, case).
        auto missing = previousVersionFromDeployHistory(fixture, "WindowsPlayer", "version-zzz999");
        assert(!missing.has_value());

        // Other binary types are ignored when resolving WindowsPlayer's history.
        auto studioIgnored = previousVersionFromDeployHistory(fixture, "WindowsPlayer", "version-bbb222");
        assert(!studioIgnored.has_value()); // bbb222 is a Studio entry, not in Player's sequence
    }

    // parsePackageManifest(): fixture modeling the publicly documented
    // rbxPkgManifest.txt format (version marker line, then groups of 4:
    // filename / MD5 / packed size / unpacked size) -- NOT re-verified
    // live, see open questions.
    {
        const std::string fixture =
            "v0\r\n"
            "RobloxApp.zip\r\n"
            "d41d8cd98f00b204e9800998ecf8427e\r\n"
            "1234\r\n"
            "5678\r\n"
            "shaders.zip\r\n"
            "900150983cd24fb0d6963f7d28e17f72\r\n"
            "111\r\n"
            "222\r\n";

        auto packages = parsePackageManifest(fixture);
        assert(packages.size() == 2);
        assert(packages[0].name == "RobloxApp.zip");
        assert(packages[0].md5 == "d41d8cd98f00b204e9800998ecf8427e");
        assert(packages[0].packedSize == 1234ULL);
        assert(packages[0].unpackedSize == 5678ULL);
        assert(packages[1].name == "shaders.zip");
    }

    // packageInstallSubdir(): known packages resolve; unknown packages
    // return nullopt (never a guessed path).
    {
        assert(packageInstallSubdir("RobloxApp.zip", LaunchTarget::Player) == "");
        assert(packageInstallSubdir("shaders.zip", LaunchTarget::Player) == "shaders/");
        assert(packageInstallSubdir("ssl.zip", LaunchTarget::Player) == "ssl/");
        assert(!packageInstallSubdir("SomeFutureUnknownPackage.zip", LaunchTarget::Player).has_value());
    }

    // setupCdnUrl(): builds a fetchable URL for a given mirror.
    {
        assert(setupCdnUrl("setup.rbxcdn.com", "version-abc123", "RobloxApp.zip") ==
               "https://setup.rbxcdn.com/version-abc123-RobloxApp.zip");
    }

    printf("roblox_deploy: all tests passed\n");
    return 0;
}
