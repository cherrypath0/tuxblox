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
        parseManifest(R"({"manifest_version": 1, "tuxblox_version": "0.1.0", "artifacts": {}})");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Test missing leaf field (well-formed JSON but missing required artifact field)
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 1,
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            },
            "installer": {
              "url": "https://example.com/installer",
              "sha256": "12345678",
              "size_bytes": 1
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Test missing installer artifact entirely.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 1,
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "sha256": "deadbeef",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Test wrong type for manifest_version (string instead of int)
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": "1",
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "sha256": "deadbeef",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            },
            "installer": {
              "url": "https://example.com/installer",
              "sha256": "12345678",
              "size_bytes": 1
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Test missing top-level manifest_version field
    threw = false;
    try {
        parseManifest(R"({
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "sha256": "deadbeef",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            },
            "installer": {
              "url": "https://example.com/installer",
              "sha256": "12345678",
              "size_bytes": 1
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // Test unsupported manifest_version (future schema bump must fail loudly,
    // not be silently misparsed by an older installer)
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 2,
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "sha256": "deadbeef",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            },
            "installer": {
              "url": "https://example.com/installer",
              "sha256": "12345678",
              "size_bytes": 1
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    // A zero/negative version is equally unsupported.
    threw = false;
    try {
        parseManifest(R"({
          "manifest_version": 0,
          "tuxblox_version": "0.1.0",
          "artifacts": {
            "protonbuild": {
              "url": "https://example.com/proton.tar.gz",
              "sha256": "deadbeef",
              "size_bytes": 123456789
            },
            "launcher": {
              "url": "https://example.com/launcher",
              "sha256": "cafebabe",
              "size_bytes": 12345678
            },
            "installer": {
              "url": "https://example.com/installer",
              "sha256": "12345678",
              "size_bytes": 1
            }
          }
        })");
    } catch (const std::runtime_error&) {
        threw = true;
    }
    assert(threw);

    printf("manifest: all tests passed\n");
    return 0;
}
