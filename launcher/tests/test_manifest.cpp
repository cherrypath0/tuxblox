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
          "url": "/v1/canary/0.2.0/proton.tar.zst"
        },
        "launcher": {
          "size": 12345678,
          "sha256": "cafebabe",
          "url": "/v1/canary/0.2.0/launcher"
        },
        "installer": {
          "size": 23456789,
          "sha256": "12345678",
          "url": "/v1/canary/0.2.0/installer"
        }
      }
    })";

    // Artifact urls are manifest-relative -- parseManifest must resolve them
    // against baseUrl, not hand back the raw JSON string.
    auto m = parseManifest(good, baseUrl);
    assert(m.manifestVersion == 2);
    assert(m.channel == "canary");
    assert(m.proton.url == "https://setup.tuxblox.net/v1/canary/0.2.0/proton.tar.zst");
    assert(m.proton.sha256 == "deadbeef");
    assert(m.proton.sizeBytes == 123456789ULL);
    assert(m.launcher.url == "https://setup.tuxblox.net/v1/canary/0.2.0/launcher");
    assert(m.launcher.sha256 == "cafebabe");
    assert(m.launcher.sizeBytes == 12345678ULL);
    assert(m.installer.url == "https://setup.tuxblox.net/v1/canary/0.2.0/installer");
    assert(m.installer.sha256 == "12345678");
    assert(m.installer.sizeBytes == 23456789ULL);

    // An artifact url that's already absolute is used as-is (defensive --
    // the server never emits this today, but shouldn't be double-prefixed
    // if it ever did).
    {
        const std::string absoluteUrlManifest = R"({
          "channel": "canary",
          "manifest_version": 2,
          "artifacts": {
            "proton": {"size": 1, "sha256": "a", "url": "https://example.com/proton.tar.zst"},
            "launcher": {"size": 1, "sha256": "b", "url": "/v1/canary/0.2.0/launcher"},
            "installer": {"size": 1, "sha256": "c", "url": "/v1/canary/0.2.0/installer"}
          }
        })";
        auto m2 = parseManifest(absoluteUrlManifest, baseUrl);
        assert(m2.proton.url == "https://example.com/proton.tar.zst");
    }

    bool threw = false;
    try {
        parseManifest("{ not json", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        parseManifest(R"({"channel": "canary", "manifest_version": 2, "artifacts": {}})", baseUrl);
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
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton"},
            "launcher": {"size": 1, "sha256": "cafebabe", "url": "/launcher"},
            "installer": {"size": 1, "sha256": "12345678", "url": "/installer"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing installer artifact entirely.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": 2,
          "artifacts": {
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton"},
            "launcher": {"size": 1, "sha256": "cafebabe", "url": "/launcher"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing leaf artifact field.
    threw = false;
    try {
        parseManifest(R"({
          "channel": "canary",
          "manifest_version": 2,
          "artifacts": {
            "proton": {"url": "/proton", "size": 123456789},
            "launcher": {"size": 1, "sha256": "cafebabe", "url": "/launcher"},
            "installer": {"size": 1, "sha256": "12345678", "url": "/installer"}
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
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton"},
            "launcher": {"size": 1, "sha256": "cafebabe", "url": "/launcher"},
            "installer": {"size": 1, "sha256": "12345678", "url": "/installer"}
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
            "proton": {"size": 1, "sha256": "deadbeef", "url": "/proton"},
            "launcher": {"size": 1, "sha256": "cafebabe", "url": "/launcher"},
            "installer": {"size": 1, "sha256": "12345678", "url": "/installer"}
          }
        })", baseUrl);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    printf("manifest: parseManifest tests passed\n");

    // --- fetchLatestVersion ---
    {
        fs::path work = fs::temp_directory_path() / "tuxblox_test_manifest_latest";
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
