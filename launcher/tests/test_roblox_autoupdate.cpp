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

#include "roblox_autoupdate.h"
#include "versions_manifest.h"
#include <cassert>
#include <vector>
#include <filesystem>
#include <cstdio>
#include <optional>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // With a drive on disk, the paths follow the account folder that is in it.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_account";
        fs::remove_all(base);
        fs::create_directories(base / "runtime/pfx/drive_c/users/cherry");
        assert(robloxVersionsDir(base.string()) ==
               (base / "runtime/pfx/drive_c/users/cherry/AppData/Local/Roblox/Versions").string());
        fs::remove_all(base);
    }

    assert(robloxVersionsDir("/home/a/.tuxblox") ==
           "/home/a/.tuxblox/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions");

    // The setting is now the only thing that decides: the program it runs is this one.
    {
        Settings on;
        on.autoUpdateRoblox = true;
        Settings off;
        off.autoUpdateRoblox = false;
        assert(shouldRunUpdateCheck(on));
        assert(!shouldRunUpdateCheck(off));

        // Resolution is /proc/self/exe and nothing else; a readable one is this test binary.
        assert(ownExePath().has_value());
        assert(!ownExePath()->empty());
        assert(ownExePath()->front() == '/');

        // An unresolvable executable skips the check rather than blocking the launch.
        const std::optional<std::string> none;
        const std::optional<std::string> some{"/usr/bin/true"};
        assert(canRunUpdateCheck(on, some));
        assert(!canRunUpdateCheck(on, none));
        assert(!canRunUpdateCheck(off, some));
        assert(!canRunUpdateCheck(off, none));
    }

    // The environment handed to the child names the app being launched, so
    // --update-roblox resolves the right binaryType.
    {
        const auto player = updateEnvironment("/home/a/.tuxblox", LaunchTarget::Player);
        bool sawApp = false, sawDir = false;
        for (const auto& pair : player) {
            if (pair == "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=player") sawApp = true;
            if (pair == "TUXBLOX_BOOTSTRAPPER_INSTALL_DIR=/home/a/.tuxblox/runtime/pfx/drive_c/"
                        "users/user/AppData/Local/Roblox/Versions") sawDir = true;
        }
        assert(sawApp && sawDir);

        const auto studio = updateEnvironment("/home/a/.tuxblox", LaunchTarget::Studio);
        bool sawStudio = false;
        for (const auto& pair : studio) {
            if (pair == "TUXBLOX_BOOTSTRAPPER_DOWNLOAD_APP=studio") sawStudio = true;
        }
        assert(sawStudio);
    }

    // removeSupersededVersions() deletes the other builds of ONE app, and nothing else.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_prune";
        fs::remove_all(base);
        const fs::path versions = fs::path(robloxVersionsDir(base.string()));
        fs::create_directories(versions);

        auto makeVersion = [&](const char* hash, const char* exe) {
            fs::create_directories(versions / hash);
            FILE* f = fopen((versions / hash / exe).string().c_str(), "w");
            assert(f);
            fclose(f);
        };
        auto makeArchive = [&](const char* hash) {
            FILE* f = fopen((versions / (std::string(hash) + ".zip")).string().c_str(), "w");
            assert(f);
            fclose(f);
        };

        makeVersion("version-keep", "RobloxPlayerBeta.exe");
        makeVersion("version-old", "RobloxPlayerBeta.exe");
        makeVersion("version-studio", "RobloxStudioBeta.exe");
        makeArchive("version-old");
        makeArchive("version-studio");

        removeSupersededVersions(base.string(), LaunchTarget::Player, "version-keep");

        assert(fs::exists(versions / "version-keep"));          // the one just installed stays
        assert(!fs::exists(versions / "version-old"));          // the superseded Player build goes
        assert(!fs::exists(versions / "version-old.zip"));      // and so does its leftover archive
        assert(fs::exists(versions / "version-studio"));        // the other app is never touched
        assert(fs::exists(versions / "version-studio.zip"));

        // An update that installed nothing must not delete anything: the active version is then
        // one the user chose on purpose.
        makeVersion("version-second", "RobloxPlayerBeta.exe");
        removeSupersededVersions(base.string(), LaunchTarget::Player, "");
        assert(fs::exists(versions / "version-keep"));
        assert(fs::exists(versions / "version-second"));

        fs::remove_all(base);
    }


    // The auto-update may tidy away a build it installed itself. It may NOT tidy away
    // one somebody picked on purpose -- that is how installing an earlier build to get
    // away from a broken one would silently put the broken one back on the next launch.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_chosen";
        fs::remove_all(base);
        const fs::path versions = fs::path(robloxVersionsDir(base.string()));
        fs::create_directories(versions);

        auto makeVersion = [&](const char* hash) {
            fs::create_directories(versions / hash);
            FILE* f = fopen((versions / hash / "RobloxPlayerBeta.exe").string().c_str(), "w");
            assert(f);
            fclose(f);
        };
        auto seed = [&](const char* active, std::vector<InstalledVersion> installed) {
            VersionsManifest m;
            m.player.activeHash = active;
            m.player.installed = std::move(installed);
            saveVersionsManifest(base.string(), m);
        };

        makeVersion("version-latest");
        makeVersion("version-next");
        makeVersion("version-stale");
        seed("version-latest", {
            {"version-latest", "live", "", "", "latest"},
            {"version-next", "zbeta", "", "", "next"},
            {"version-stale", "live", "", "", "auto"},
        });

        removeSupersededVersions(base.string(), LaunchTarget::Player, "version-latest");

        assert(fs::exists(versions / "version-latest"));  // just installed, and active
        assert(fs::exists(versions / "version-next"));    // chosen on purpose -- not the updater's to remove
        assert(!fs::exists(versions / "version-stale")); // ordinary superseded build, still collected
        fs::remove_all(base);
    }

    // The rule is "the latest build, the pending build, or the one currently active" --
    // and nothing wider. An earlier build installed on purpose and then switched away
    // from is an ordinary superseded build again, or a fortnight of debugging leaves
    // several gigabytes nothing will ever collect.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_narrow";
        fs::remove_all(base);
        const fs::path versions = fs::path(robloxVersionsDir(base.string()));
        fs::create_directories(versions);
        for (const char* hash : {"version-latest", "version-oldpick"}) {
            fs::create_directories(versions / hash);
            FILE* f = fopen((versions / hash / "RobloxPlayerBeta.exe").string().c_str(), "w");
            assert(f);
            fclose(f);
        }
        VersionsManifest m;
        m.player.activeHash = "version-latest";
        m.player.installed = {{"version-latest", "live", "", "", "latest"},
                              {"version-oldpick", "live", "", "", "previous"}};
        saveVersionsManifest(base.string(), m);

        removeSupersededVersions(base.string(), LaunchTarget::Player, "version-latest");
        assert(!fs::exists(versions / "version-oldpick"));
        fs::remove_all(base);
    }

    // The build that is active is kept even when the updater just installed another.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_keepactive";
        fs::remove_all(base);
        const fs::path versions = fs::path(robloxVersionsDir(base.string()));
        fs::create_directories(versions);
        for (const char* hash : {"version-latest", "version-active"}) {
            fs::create_directories(versions / hash);
            FILE* f = fopen((versions / hash / "RobloxPlayerBeta.exe").string().c_str(), "w");
            assert(f);
            fclose(f);
        }
        VersionsManifest m;
        m.player.activeHash = "version-active";
        m.player.installed = {{"version-latest", "live", "", "", "latest"},
                              {"version-active", "live", "", "", "previous"}};
        saveVersionsManifest(base.string(), m);

        removeSupersededVersions(base.string(), LaunchTarget::Player, "version-latest");
        assert(fs::exists(versions / "version-active"));
        fs::remove_all(base);
    }

    // activateNewlyUpdatedVersion(): the pin follows the update only when the update
    // is what put the active build there. Without this half, protecting the active
    // build from deletion protects nothing -- it stops being active on this launch and
    // is an ordinary superseded build by the next one.
    {
        const fs::path base = fs::temp_directory_path() / "tuxblox_test_autoupdate_pin";
        fs::remove_all(base);
        const fs::path versions = fs::path(robloxVersionsDir(base.string()));
        fs::create_directories(versions);
        auto makeVersion = [&](const char* hash) {
            fs::create_directories(versions / hash);
            FILE* f = fopen((versions / hash / "RobloxPlayerBeta.exe").string().c_str(), "w");
            assert(f);
            fclose(f);
        };

        // Chosen on purpose: the newly installed build does not take the pin.
        makeVersion("version-chosen");
        VersionsManifest m;
        m.player.activeHash = "version-chosen";
        m.player.installed = {{"version-chosen", "live", "", "", "previous"}};
        saveVersionsManifest(base.string(), m);
        const std::vector<std::string> before = scanPrefixVersions(base.string(), LaunchTarget::Player);
        makeVersion("version-fresh");
        activateNewlyUpdatedVersion(base.string(), LaunchTarget::Player, before);
        assert(loadInstalledVersions(base.string()).player.activeHash == "version-chosen");

        // The updater's own build: the pin follows, exactly as it does today.
        VersionsManifest auto_m;
        auto_m.player.activeHash = "version-chosen";
        auto_m.player.installed = {{"version-chosen", "live", "", "", "auto"},
                                   {"version-fresh", "live", "", "", "auto"}};
        saveVersionsManifest(base.string(), auto_m);
        makeVersion("version-newer");
        const std::vector<std::string> before2 = {"version-chosen", "version-fresh"};
        activateNewlyUpdatedVersion(base.string(), LaunchTarget::Player, before2);
        assert(loadInstalledVersions(base.string()).player.activeHash == "version-newer");
        fs::remove_all(base);
    }

    printf("roblox_autoupdate: all tests passed\n");
    return 0;
}
