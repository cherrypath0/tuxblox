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

#include "versions_manifest.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    fs::path dir = fs::temp_directory_path() / "tuxblox_test_versions_manifest";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // Missing file -> defaults (empty, nothing active, not bootstrapped).
    {
        VersionsManifest m = loadVersionsManifest(dir.string());
        assert(m.player.installed.empty());
        assert(m.player.activeHash.empty());
        assert(!m.player.bootstrapped);
        assert(m.studio.installed.empty());
    }

    // Round-trip: save then load must reproduce every field.
    {
        VersionsManifest m;
        m.player.installed.push_back({"version-abc123", "live", "2026-08-16T00:00:00Z"});
        m.player.installed.push_back({"version-def456", "live", "2026-08-15T00:00:00Z"});
        m.player.activeHash = "version-abc123";
        m.player.bootstrapped = true;
        m.studio.bootstrapped = false;
        saveVersionsManifest(dir.string(), m);

        VersionsManifest loaded = loadVersionsManifest(dir.string());
        assert(loaded.player.installed.size() == 2);
        assert(loaded.player.installed[0].hash == "version-abc123");
        assert(loaded.player.installed[0].channel == "live");
        assert(loaded.player.installed[0].installedAt == "2026-08-16T00:00:00Z");
        assert(loaded.player.installed[1].hash == "version-def456");
        assert(loaded.player.activeHash == "version-abc123");
        assert(loaded.player.bootstrapped);
        assert(!loaded.studio.bootstrapped);
    }

    // Malformed JSON -> defaults, never throws.
    {
        std::ofstream bad(dir / "versions.json", std::ios::binary);
        bad << "{ not valid json";
        bad.close();
        VersionsManifest m = loadVersionsManifest(dir.string());
        assert(m.player.installed.empty());
    }

    // appVersionsFor() selects the right struct for each LaunchTarget.
    {
        VersionsManifest m;
        appVersionsFor(m, LaunchTarget::Player).activeHash = "version-p";
        appVersionsFor(m, LaunchTarget::Studio).activeHash = "version-s";
        assert(appVersionsFor(m, LaunchTarget::Player).activeHash == "version-p");
        assert(appVersionsFor(m, LaunchTarget::Studio).activeHash == "version-s");
        const VersionsManifest& cm = m;
        assert(appVersionsFor(cm, LaunchTarget::Player).activeHash == "version-p");
    }

    // registerBootstrappedVersion(): scans versions/ for the single
    // directory the official installer just created, records it as
    // installed+bootstrapped, and pins it active (since nothing was active
    // before -- this is the first-ever install for this app type).
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_versions_manifest_bootstrap";
        fs::remove_all(dir);
        fs::path versionsDir = dir / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
        fs::create_directories(versionsDir / "version-freshinstall");
        // A directory only counts as "newly discovered" for a target if it
        // actually contains that target's exe (Finding 3 fix) -- so the
        // fixture must include it, not just the bare directory.
        std::ofstream(versionsDir / "version-freshinstall" / "RobloxPlayerBeta.exe") << "fake exe";

        registerBootstrappedVersion(dir.string(), LaunchTarget::Player);

        VersionsManifest m = loadVersionsManifest(dir.string());
        assert(m.player.bootstrapped);
        assert(m.player.activeHash == "version-freshinstall");
        assert(m.player.installed.size() == 1);
        assert(m.player.installed[0].hash == "version-freshinstall");
        fs::remove_all(dir);
    }

    // registerBootstrappedVersion(): no versions/ directory at all (the
    // installer run failed to actually install anything) -> no-op, doesn't
    // throw, doesn't mark bootstrapped.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_versions_manifest_bootstrap_empty";
        fs::remove_all(dir);
        fs::create_directories(dir);
        registerBootstrappedVersion(dir.string(), LaunchTarget::Player);
        VersionsManifest m = loadVersionsManifest(dir.string());
        assert(!m.player.bootstrapped);
        fs::remove_all(dir);
    }

    // registerBootstrappedVersion(): must merge, not clobber. Seed an
    // existing versions.json with a pre-existing pinned entry for Player
    // AND a completely separate entry for Studio, then run the bootstrap
    // scan against a versions/ dir containing both that already-known
    // directory and one genuinely new one. The pre-existing Player entry
    // and its active pin must survive unchanged, the new entry must be
    // appended (not replace it), and Studio's data must be untouched.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_versions_manifest_bootstrap_merge";
        fs::remove_all(dir);
        fs::create_directories(dir);

        VersionsManifest seed;
        seed.player.installed.push_back({"version-existing", "live", "2026-08-01T00:00:00Z"});
        seed.player.activeHash = "version-existing";
        seed.player.bootstrapped = false;
        seed.studio.installed.push_back({"version-studio-existing", "live", "2026-07-01T00:00:00Z"});
        seed.studio.activeHash = "version-studio-existing";
        seed.studio.bootstrapped = false;
        saveVersionsManifest(dir.string(), seed);

        fs::path versionsDir = dir / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
        fs::create_directories(versionsDir / "version-existing");   // already known -- must not duplicate
        fs::create_directories(versionsDir / "version-newfound");   // genuinely new -- must be appended
        // "version-existing" is skipped via the alreadyKnown check before the
        // exe-presence check even runs, so it doesn't need an exe file here --
        // but "version-newfound" does (Finding 3 fix).
        std::ofstream(versionsDir / "version-newfound" / "RobloxPlayerBeta.exe") << "fake exe";

        registerBootstrappedVersion(dir.string(), LaunchTarget::Player);

        VersionsManifest m = loadVersionsManifest(dir.string());

        // Pre-existing Player entry survives unchanged.
        assert(m.player.installed.size() == 2);
        bool foundExisting = false, foundNew = false;
        for (const auto& iv : m.player.installed) {
            if (iv.hash == "version-existing") {
                foundExisting = true;
                assert(iv.channel == "live");
                assert(iv.installedAt == "2026-08-01T00:00:00Z");
            } else if (iv.hash == "version-newfound") {
                foundNew = true;
            }
        }
        assert(foundExisting);
        assert(foundNew);
        // Active pin was already set -- must not be clobbered/reassigned.
        assert(m.player.activeHash == "version-existing");
        assert(m.player.bootstrapped);

        // Studio's data is completely untouched.
        assert(m.studio.installed.size() == 1);
        assert(m.studio.installed[0].hash == "version-studio-existing");
        assert(m.studio.installed[0].channel == "live");
        assert(m.studio.installed[0].installedAt == "2026-07-01T00:00:00Z");
        assert(m.studio.activeHash == "version-studio-existing");
        assert(!m.studio.bootstrapped);

        fs::remove_all(dir);
    }

    // registerBootstrappedVersion(): cross-target contamination (Finding 3,
    // 2026-08-16 final review). Player and Studio versions live in the SAME
    // shared Versions/ directory. Seed Player as already registered against
    // directory "version-player-existing" (containing RobloxPlayerBeta.exe),
    // then bootstrap Studio against a Versions/ dir containing that same
    // Player directory PLUS one genuinely new Studio directory (containing
    // RobloxStudioBeta.exe). Studio's scan must see exactly one new hash
    // (its own), not two -- the Player directory must not be misdetected as
    // "new" just because it's absent from Studio's own installed list.
    {
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_versions_manifest_bootstrap_cross_target";
        fs::remove_all(dir);
        fs::create_directories(dir);

        VersionsManifest seed;
        seed.player.installed.push_back({"version-player-existing", "live", "2026-08-01T00:00:00Z"});
        seed.player.activeHash = "version-player-existing";
        seed.player.bootstrapped = true;
        saveVersionsManifest(dir.string(), seed);

        fs::path versionsDir = dir / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions";
        fs::create_directories(versionsDir / "version-player-existing");
        std::ofstream(versionsDir / "version-player-existing" / "RobloxPlayerBeta.exe") << "fake exe";
        fs::create_directories(versionsDir / "version-studio-newfound");
        std::ofstream(versionsDir / "version-studio-newfound" / "RobloxStudioBeta.exe") << "fake exe";

        registerBootstrappedVersion(dir.string(), LaunchTarget::Studio);

        VersionsManifest m = loadVersionsManifest(dir.string());

        // Exactly one new hash found for Studio -> auto-pinned active (the
        // "exactly one new hash" logic that cross-target contamination
        // would otherwise break by inflating the count to two).
        assert(m.studio.installed.size() == 1);
        assert(m.studio.installed[0].hash == "version-studio-newfound");
        assert(m.studio.activeHash == "version-studio-newfound");
        assert(m.studio.bootstrapped);

        // Player's own data is untouched by Studio's bootstrap scan.
        assert(m.player.installed.size() == 1);
        assert(m.player.installed[0].hash == "version-player-existing");
        assert(m.player.activeHash == "version-player-existing");

        fs::remove_all(dir);
    }

    fs::remove_all(dir);
    printf("versions_manifest: all tests passed\n");
    return 0;
}
