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

        // Studio's own executable and its Qt runtime -- without these the
        // version has no RobloxStudioBeta.exe at all.
        // WebView2Loader.dll sits beside the executable, not in a subfolder.
        assert(packageInstallSubdir("WebView2.zip", LaunchTarget::Player) == "");
        assert(packageInstallSubdir("RobloxStudio.zip", LaunchTarget::Player) == "");
        assert(packageInstallSubdir("Libraries.zip", LaunchTarget::Player) == "");
        assert(packageInstallSubdir("LibrariesQt5.zip", LaunchTarget::Player) == "");
        assert(packageInstallSubdir("BuiltInStandalonePlugins.zip", LaunchTarget::Player) ==
               "BuiltInStandalonePlugins/");
        assert(packageInstallSubdir("content-qt_translations.zip", LaunchTarget::Player) ==
               "content/qt_translations/");
        assert(packageInstallSubdir("content-studio_svg_textures.zip", LaunchTarget::Player) ==
               "content/studio_svg_textures/");
        assert(packageInstallSubdir("content-api-docs.zip", LaunchTarget::Player) == "content/api_docs/");
        assert(packageInstallSubdir("content-platform-dictionaries.zip", LaunchTarget::Player) ==
               "PlatformContent/pc/shared_compression_dictionaries/");
        assert(packageInstallSubdir("extracontent-scripts.zip", LaunchTarget::Player) == "ExtraContent/scripts/");
        assert(packageInstallSubdir("studiocontent-models.zip", LaunchTarget::Player) == "StudioContent/models/");
        assert(packageInstallSubdir("studiocontent-textures.zip", LaunchTarget::Player) ==
               "StudioContent/textures/");
        // Holds MicrosoftEdgeWebview2Setup.exe, which TuxBlox's own WebView2
        // support makes unnecessary. Skipped on purpose, not unknown.
        assert(!packageInstallSubdir("WebView2RuntimeInstaller.zip", LaunchTarget::Player).has_value());

    }

    // normalizeChannel(): blank means the default channel, and channel
    // names on the CDN are lowercase only.
    {
        assert(normalizeChannel("") == "live");
        assert(normalizeChannel("live") == "live");
        assert(normalizeChannel("LIVE") == "live");
        assert(normalizeChannel("ZIntegration") == "zintegration");
        assert(normalizeChannel("  zcanary  ") == "zcanary");
    }

    // setupCdnUrl(): the default channel lives at the mirror root, every
    // other channel under channel/<name>/.
    {
        assert(setupCdnUrl("setup.rbxcdn.com", "live", "version-abc123", "RobloxApp.zip") ==
               "https://setup.rbxcdn.com/version-abc123-RobloxApp.zip");
        assert(setupCdnUrl("setup.rbxcdn.com", "", "version-abc123", "RobloxApp.zip") ==
               "https://setup.rbxcdn.com/version-abc123-RobloxApp.zip");
        assert(setupCdnUrl("setup.rbxcdn.com", "zcanary", "version-abc123", "RobloxApp.zip") ==
               "https://setup.rbxcdn.com/channel/zcanary/version-abc123-RobloxApp.zip");
        assert(setupCdnUrl("setup-aws.rbxcdn.com", "ZCanary", "version-abc123", "rbxPkgManifest.txt") ==
               "https://setup-aws.rbxcdn.com/channel/zcanary/version-abc123-rbxPkgManifest.txt");
    }

    // normalizeChannel(): "production" is the name people reach for; Roblox's own is
    // "live", and resolving it here keeps one answer rather than two.
    {
        assert(normalizeChannel("production") == "live");
        assert(normalizeChannel("PRODUCTION") == "live");
        assert(normalizeChannel("  Production  ") == "live");
        assert(normalizeChannel("live") == "live");
        assert(normalizeChannel("LIVE") == "live");
        assert(normalizeChannel("") == "live");
        // Only the exact name is an alias -- nothing else is rewritten.
        assert(normalizeChannel("zbeta") == "zbeta");
        assert(normalizeChannel("production2") == "production2");
        assert(normalizeChannel("preproduction") == "preproduction");
    }

    // An aliased channel must reach the CDN root, not channel/production/.
    {
        assert(setupCdnUrl("setup.rbxcdn.com", "production", "version-abc123", "rbxPkgManifest.txt") ==
               "https://setup.rbxcdn.com/version-abc123-rbxPkgManifest.txt");
        assert(deployHistoryUrl("setup.rbxcdn.com", "Production") ==
               "https://setup.rbxcdn.com/DeployHistory.txt");
    }

    // deployHistoryUrl(): same split as setupCdnUrl().
    {
        assert(deployHistoryUrl("setup.rbxcdn.com", "live") ==
               "https://setup.rbxcdn.com/DeployHistory.txt");
        assert(deployHistoryUrl("setup.rbxcdn.com", "zintegration") ==
               "https://setup.rbxcdn.com/channel/zintegration/DeployHistory.txt");
    }

    // comparePendingToLive(): which of the three states Roblox's zbeta channel is in.
    {
        assert(comparePendingToLive("0.747.0.7471481", "0.746.0.7461267") == PendingBuild::Newer);
        assert(comparePendingToLive("0.746.0.7461267", "0.746.0.7461267") == PendingBuild::SameAsLive);
        assert(comparePendingToLive("0.745.0.7450000", "0.746.0.7461267") == PendingBuild::OlderThanLive);
        // Field-wise and numeric, not string order: "1000" sorts before "999" as text.
        assert(comparePendingToLive("0.1000.0.1", "0.999.0.1") == PendingBuild::Newer);
        assert(comparePendingToLive("0.746.0.7461268", "0.746.0.7461267") == PendingBuild::Newer);
        // A version that cannot be read is never treated as newer -- it must not install.
        assert(comparePendingToLive("", "0.746.0.7461267") == PendingBuild::OlderThanLive);
        assert(comparePendingToLive("nonsense", "0.746.0.7461267") == PendingBuild::OlderThanLive);
        assert(comparePendingToLive("0.747", "0.746.0.7461267") == PendingBuild::OlderThanLive);
        assert(comparePendingToLive("0.747.0.7471481", "") == PendingBuild::OlderThanLive);
    }

    // parseClientVersionNumber(): the number beside the hash. Needed because deciding
    // whether an unreleased build is actually newer is a comparison of versions, and the
    // hash says nothing about order.
    {
        const std::string fixture = R"({"version":"0.747.0.7471481",)"
                                    R"("clientVersionUpload":"version-4ea8e85481e34f46",)"
                                    R"("bootstrapperVersion":""})";
        assert(parseClientVersionNumber(fixture) == "0.747.0.7471481");
        bool threw = false;
        try { parseClientVersionNumber(R"({"clientVersionUpload":"version-abc"})"); }
        catch (const std::runtime_error&) { threw = true; }
        assert(threw);
    }

    // isNotFound(): the version history answers 404 when it has no earlier build recorded,
    // which is a thing to explain rather than an error to report.
    {
        assert(isNotFound("httpGet: HTTP 404 for https://config.tuxblox.net/..."));
        assert(!isNotFound("httpGet: HTTP 401 for https://config.tuxblox.net/..."));
        assert(!isNotFound("httpGet: Could not resolve host: config.tuxblox.net"));
        assert(!isNotFound(""));
    }

    // pendingChannelsInOrder(): where a deployed-but-unreleased build's files actually live.
    // Measured 2026-11-10: the live CDN root served 0.747's manifest with HTTP 200 while
    // channel/zbeta/ answered 403, so the root is the real location and the channel path is
    // a fallback against a future change in Roblox's layout.
    {
        const std::vector<std::string> channels = pendingChannelsInOrder();
        assert(channels.size() == 2);
        assert(channels[0] == "live");
        assert(channels[1] == "zbeta");
        assert(setupCdnUrl("setup.rbxcdn.com", channels[0], "version-4ea8e85481e34f46", "rbxPkgManifest.txt") ==
               "https://setup.rbxcdn.com/version-4ea8e85481e34f46-rbxPkgManifest.txt");
        assert(setupCdnUrl("setup.rbxcdn.com", channels[1], "version-4ea8e85481e34f46", "rbxPkgManifest.txt") ==
               "https://setup.rbxcdn.com/channel/zbeta/version-4ea8e85481e34f46-rbxPkgManifest.txt");
    }

    // The matchers must read what httpGet actually throws, not a string invented here.
    // CURLOPT_FAILONERROR used to abort the transfer on a 4xx, so the status message was
    // never built and every one of these matched nothing in the field.
    {
        assert(isNotAuthorized(httpStatusErrorText(401, "https://clientsettings.roblox.com/x")));
        assert(isNotFound(httpStatusErrorText(404, "https://config.tuxblox.net/x")));
        assert(!isNotAuthorized(httpStatusErrorText(404, "https://config.tuxblox.net/x")));
        assert(!isNotFound(httpStatusErrorText(401, "https://clientsettings.roblox.com/x")));
        // The URL is named, which is what makes a failure diagnosable at all.
        assert(httpStatusErrorText(404, "https://setup.rbxcdn.com/a").find(
                   "https://setup.rbxcdn.com/a") != std::string::npos);
        // A 401 appearing in the URL rather than the status must not match.
        assert(!isNotAuthorized(httpStatusErrorText(200, "https://example.invalid/401")));
    }

    // isNotAuthorized(): measured 2026-11-10, zbeta answers only while a build is pending.
    // Once 0.747 was promoted it began refusing both binaries, so this is the ordinary
    // "nothing upcoming" state and must not be reported as a channel the user got wrong.
    {
        assert(isNotAuthorized("httpGet: HTTP 401 for https://clientsettings.roblox.com/..."));
        assert(!isNotAuthorized("httpGet: HTTP 403 for https://clientsettings.roblox.com/..."));
        assert(!isNotAuthorized("httpGet: HTTP 404 for https://setup.rbxcdn.com/..."));
        assert(!isNotAuthorized("httpGet: Could not resolve host"));
        assert(!isNotAuthorized(""));
        // Not fooled by a 401 appearing somewhere that is not the status.
        assert(!isNotAuthorized("httpGet: HTTP 200 for https://example.invalid/401"));
    }

    printf("roblox_deploy: all tests passed\n");
    return 0;
}
