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
#include <stdexcept>
#include <string>

int main() {
    using tuxblox::parseManifest;

    const std::string good = R"({
      "manifest_version": 1,
      "tuxblox_version": "0.1.0",
      "proton_version": "0.1.0",
      "artifacts": {
        "protonbuild": {
          "url": "https://assetdelivery.tuxblox.net/pkg/protonbuild-0-1-0.tar.gz",
          "sha256": "deadbeef",
          "size_bytes": 123456789
        },
        "launcher": {
          "url": "https://assetdelivery.tuxblox.net/pkg/TuxBloxLauncher",
          "sha256": "cafebabe",
          "size_bytes": 12345678
        },
        "installer": {
          "url": "https://assetdelivery.tuxblox.net/pkg/TuxBloxInstaller",
          "sha256": "12345678",
          "size_bytes": 23456789
        }
      }
    })";

    auto m = parseManifest(good);
    assert(m.manifestVersion == 1);
    assert(m.tuxbloxVersion == "0.1.0");
    assert(m.protonVersion == "0.1.0");
    assert(m.protonbuild.url == "https://assetdelivery.tuxblox.net/pkg/protonbuild-0-1-0.tar.gz");
    assert(m.protonbuild.sha256 == "deadbeef");
    assert(m.protonbuild.sizeBytes == 123456789ULL);
    assert(m.launcher.url == "https://assetdelivery.tuxblox.net/pkg/TuxBloxLauncher");
    assert(m.launcher.sha256 == "cafebabe");
    assert(m.launcher.sizeBytes == 12345678ULL);
    assert(m.installer.url == "https://assetdelivery.tuxblox.net/pkg/TuxBloxInstaller");
    assert(m.installer.sha256 == "12345678");
    assert(m.installer.sizeBytes == 23456789ULL);

    bool threw = false;
    try {
        parseManifest("{ not json");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        parseManifest(R"({"manifest_version": 1, "tuxblox_version": "0.1.0", "proton_version": "0.1.0", "artifacts": {}})");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing proton_version (well-formed JSON, but the field this task adds is absent).
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 1,
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {"url": "https://example.com/proton.tar.gz", "sha256": "deadbeef", "size_bytes": 1},
            "launcher": {"url": "https://example.com/launcher", "sha256": "cafebabe", "size_bytes": 1},
            "installer": {"url": "https://example.com/installer", "sha256": "12345678", "size_bytes": 1}
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing installer artifact entirely.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 1,
          "tuxblox_version": "0.1.0",
          "proton_version": "0.1.0",
          "artifacts": {
            "protonbuild": {"url": "https://example.com/proton.tar.gz", "sha256": "deadbeef", "size_bytes": 1},
            "launcher": {"url": "https://example.com/launcher", "sha256": "cafebabe", "size_bytes": 1}
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Missing leaf artifact field.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 1,
          "tuxblox_version": "0.1.0",
          "proton_version": "0.1.0",
          "artifacts": {
            "protonbuild": {"url": "https://example.com/proton.tar.gz", "size_bytes": 123456789},
            "launcher": {"url": "https://example.com/launcher", "sha256": "cafebabe", "size_bytes": 12345678},
            "installer": {"url": "https://example.com/installer", "sha256": "12345678", "size_bytes": 1}
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Wrong type for manifest_version.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": "1",
          "tuxblox_version": "0.1.0",
          "proton_version": "0.1.0",
          "artifacts": {
            "protonbuild": {"url": "https://example.com/proton.tar.gz", "sha256": "deadbeef", "size_bytes": 123456789},
            "launcher": {"url": "https://example.com/launcher", "sha256": "cafebabe", "size_bytes": 12345678},
            "installer": {"url": "https://example.com/installer", "sha256": "12345678", "size_bytes": 1}
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Unsupported manifest_version.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 2,
          "tuxblox_version": "0.1.0",
          "proton_version": "0.1.0",
          "artifacts": {
            "protonbuild": {"url": "https://example.com/proton.tar.gz", "sha256": "deadbeef", "size_bytes": 123456789},
            "launcher": {"url": "https://example.com/launcher", "sha256": "cafebabe", "size_bytes": 12345678},
            "installer": {"url": "https://example.com/installer", "sha256": "12345678", "size_bytes": 1}
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    printf("manifest: all tests passed\n");
    return 0;
}
