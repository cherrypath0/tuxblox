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

#include "manifest.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace fs = std::filesystem;

int main() {
    using tuxblox::fetchLatestVersion;
    using tuxblox::parseManifest;

    const std::string baseUrl = "https://setup.tuxblox.net";

    const std::string good = R"({
      "channel": "canary",
      "uploadDate": "08/08/26 15:11:00",
      "data": {"hasPlayer": false, "hasStudio": true, "isLatest": true},
      "hash": "version-abc123",
      "manifest_version": 2,
      "artifacts": {
        "proton": {
          "size": 123456789,
          "sha256": "deadbeef",
          "url": "/v1/canary/0.2.0/proton.tar.zst",
          "displayname": "Proton",
          "filename": "proton",
          "path": "/"
        },
        "launcher": {
          "size": 12345678,
          "sha256": "cafebabe",
          "url": "/v1/canary/0.2.0/launcher",
          "displayname": "Launcher",
          "filename": "TuxBloxLauncher",
          "path": "/"
        },
        "installer": {
          "size": 23456789,
          "sha256": "12345678",
          "url": "/v1/canary/0.2.0/installer",
          "displayname": "Updater",
          "filename": "TuxBloxInstaller",
          "path": "/"
        },
        "mcp": {
          "size": 338,
          "sha256": "9cd03e3d",
          "url": "/v1/canary/0.2.0/mcp.sh",
          "displayname": "Studio MCP Server",
          "filename": "mcp",
          "path": "/"
        }
      }
    })";

    // Artifact urls are manifest-relative -- parseManifest must resolve them
    // against baseUrl, not hand back the raw JSON string. artifacts is a
    // generic map, not a fixed set of named fields, so a manifest listing 4
    // artifacts (including "mcp", which neither app hardcodes anything
    // about) parses all 4 without any special-casing.
    auto m = parseManifest(good, baseUrl);
    assert(m.manifestVersion == 2);
    assert(m.channel == "canary");
    assert(m.artifacts.size() == 4);

    const auto& proton = m.artifacts.at("proton");
    assert(proton.url == "https://setup.tuxblox.net/v1/canary/0.2.0/proton.tar.zst");
    assert(proton.sha256 == "deadbeef");
    assert(proton.sizeBytes == 123456789ULL);
    assert(proton.displayname == "Proton");
    assert(proton.filename == "proton");
    assert(proton.path == "/");

    const auto& launcher = m.artifacts.at("launcher");
    assert(launcher.url == "https://setup.tuxblox.net/v1/canary/0.2.0/launcher");
    assert(launcher.sha256 == "cafebabe");
    assert(launcher.sizeBytes == 12345678ULL);
    assert(launcher.displayname == "Launcher");
    assert(launcher.filename == "TuxBloxLauncher");

    const auto& installer = m.artifacts.at("installer");
    assert(installer.url == "https://setup.tuxblox.net/v1/canary/0.2.0/installer");
    assert(installer.sha256 == "12345678");
    assert(installer.sizeBytes == 23456789ULL);
    assert(installer.displayname == "Updater");

    const auto& mcp = m.artifacts.at("mcp");
    assert(mcp.displayname == "Studio MCP Server");
    assert(mcp.filename == "mcp");

    assert(m.artifacts.find("nonexistent") == m.artifacts.end());

    // An artifact url that's already absolute is used as-is (defensive --
    // the server never emits this today, but shouldn't be double-prefixed
    // if it ever did).
    {
        const std::string absoluteUrlManifest = R"({
          "channel": "canary",
          "manifest_version": 2,
          "artifacts": {
            "proton": {"size": 1, "sha256": "a", "url": "https://example.com/proton.tar.zst",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })";
        auto m2 = parseManifest(absoluteUrlManifest, baseUrl);
        assert(m2.artifacts.at("proton").url == "https://example.com/proton.tar.zst");
    }

    // A manifest with just one artifact -- not required to have
    // launcher/installer/proton specifically, parseManifest itself doesn't
    // enforce that (only runInstall's need for a "launcher" key does).
    {
        const std::string oneArtifact = R"({
          "channel": "dev",
          "manifest_version": 2,
          "artifacts": {
            "widget": {"size": 1, "sha256": "a", "url": "/widget",
                       "displayname": "Widget", "filename": "widget", "path": "/extras"}
          }
        })";
        auto m3 = parseManifest(oneArtifact, baseUrl);
        assert(m3.artifacts.size() == 1);
        assert(m3.artifacts.at("widget").path == "/extras");
    }

    bool threw = false;
    try {
        parseManifest("{ not json", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Empty artifacts object.
    threw = false;
    try {
        parseManifest(R"({"channel": "canary", "manifest_version": 2, "artifacts": {}})", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing artifacts object entirely.
    threw = false;
    try {
        parseManifest(R"({"channel": "canary", "manifest_version": 2})", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing channel field.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 2,
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing leaf artifact field (displayname, this time -- not just the
    // pre-existing url/sha256/size trio).
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": 2,
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Wrong type for manifest_version.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": "2",
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing top-level manifest_version field.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Unsupported (old, pre-per-version) manifest_version.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": 1,
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // A zero/negative version is equally unsupported.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": 0,
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton",
                       "displayname": "Proton", "filename": "proton", "path": "/"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    printf("manifest: parseManifest tests passed\n");

    // --- fetchLatestVersion ---
    {
        fs::path work = fs::temp_directory_path() / "tuxblox_test_installer_manifest_latest";
        fs::remove_all(work);
        fs::create_directories(work / "v2");

        {
            std::ofstream out(work / "v2" / "latest.json");
            out << R"({"channels": {"stable": "", "canary": "0.2.0", "dev": ""}, "lastUpdate": "08/08/26 15:35:55"})";
        }
        const std::string fileBaseUrl = "file://" + work.string();

        auto canaryVersion = fetchLatestVersion(fileBaseUrl, "canary");
        assert(canaryVersion.has_value());
        assert(*canaryVersion == "0.2.0");

        // Empty string in latest.json -- no releases for this channel yet.
        auto stableVersion = fetchLatestVersion(fileBaseUrl, "stable");
        assert(!stableVersion.has_value());

        // Channel key not present at all in latest.json's "channels" object.
        threw = false;
        try {
            fetchLatestVersion(fileBaseUrl, "nonexistent-channel");
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);

        fs::remove_all(work);
    }

    printf("manifest: fetchLatestVersion tests passed\n");
    printf("manifest: all tests passed\n");
    return 0;
}
