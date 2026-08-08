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

#include "installer_steps.h"
#include "checksum.h"
#include <archive.h>
#include <archive_entry.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

static void makeFixtureTarZst(const std::string& path, const char* fileContent) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    struct archive_entry* entry = archive_entry_new();
    archive_entry_set_pathname(entry, "dist/proton");
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(fileContent)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0755);
    archive_write_header(a, entry);
    archive_write_data(a, fileContent, strlen(fileContent));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

int main() {
    using namespace tuxblox;

    fs::path work = fs::temp_directory_path() / "tuxblox_test_installer_steps";
    fs::remove_all(work);
    fs::create_directories(work);

    fs::path tarballSrc = work / "protonbuild.tar.zst";
    fs::path launcherSrc = work / "TuxBloxLauncherSrc";
    fs::path installerSrc = work / "TuxBloxInstallerSrc";
    fs::path robloxPlayerSrc = work / "RobloxPlayerInstallerSrc.exe";
    fs::path robloxStudioSrc = work / "RobloxStudioInstallerSrc.exe";
    fs::path installDirPath = work / "install";

    makeFixtureTarZst(tarballSrc.string(), "fake proton dist");
    {
        std::ofstream out(launcherSrc, std::ios::binary);
        out << "fake launcher binary";
    }
    {
        std::ofstream out(installerSrc, std::ios::binary);
        out << "fake installer binary";
    }
    {
        std::ofstream out(robloxPlayerSrc, std::ios::binary);
        out << "fake roblox player installer";
    }
    {
        std::ofstream out(robloxStudioSrc, std::ios::binary);
        out << "fake roblox studio installer";
    }
    const std::string robloxPlayerUrl = "file://" + robloxPlayerSrc.string();
    const std::string robloxStudioUrl = "file://" + robloxStudioSrc.string();

    Manifest manifest;
    manifest.manifestVersion = 1;
    manifest.tuxbloxVersion = "0.1.0-test";
    manifest.protonbuild.url = "file://" + tarballSrc.string();
    manifest.protonbuild.sha256 = sha256File(tarballSrc.string());
    manifest.protonbuild.sizeBytes = fs::file_size(tarballSrc);
    manifest.launcher.url = "file://" + launcherSrc.string();
    manifest.launcher.sha256 = sha256File(launcherSrc.string());
    manifest.launcher.sizeBytes = fs::file_size(launcherSrc);
    manifest.installer.url = "file://" + installerSrc.string();
    manifest.installer.sha256 = sha256File(installerSrc.string());
    manifest.installer.sizeBytes = fs::file_size(installerSrc);

    // Happy path: fresh install. Also pre-warms the Roblox Player/Studio
    // installer cache -- never runs them, only downloads and caches them
    // at the exact paths the launcher's own lazy-download fallback checks.
    {
        Step lastStep = Step::CreatingDirectory;
        double lastPercent = -1.0;
        auto outcome = runInstall(manifest,
            [&](Step step, double percent) { lastStep = step; lastPercent = percent; },
            nullptr, /*isUpgrade=*/false, installDirPath.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!outcome.cancelled);
        assert(lastStep == Step::MovingExecutables);
        assert(lastPercent > 99.9);
        assert(fs::exists(installDirPath / "steamapps"));
        assert(fs::exists(installDirPath / "runtime"));
        assert(fs::exists(installDirPath / "ProtonBuild" / "dist" / "proton"));
        assert(fs::exists(installDirPath / "TuxBloxLauncher"));
        assert(fs::exists(installDirPath / "TuxBloxInstaller"));

        fs::path playerCached = installDirPath / "RobloxPlayer" / "RobloxPlayerInstaller.exe";
        fs::path studioCached = installDirPath / "RobloxStudio" / "RobloxStudioInstaller.exe";
        assert(fs::exists(playerCached));
        assert(fs::exists(studioCached));
        {
            std::ifstream in(playerCached, std::ios::binary);
            std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
            assert(content == "fake roblox player installer");
        }
    }

    // Already-cached path: if a Roblox installer is already present (e.g.
    // from an earlier run, or the user already used it), it must not be
    // re-downloaded -- point the manifest-adjacent URL at a nonexistent
    // file so a re-download attempt would fail the whole step if it
    // incorrectly tried.
    {
        fs::path installDir2 = work / "install_roblox_cached";
        fs::path playerCached = installDir2 / "RobloxPlayer" / "RobloxPlayerInstaller.exe";
        fs::create_directories(playerCached.parent_path());
        { std::ofstream out(playerCached, std::ios::binary); out << "already cached, must survive"; }

        auto outcome = runInstall(manifest, [](Step, double) {}, nullptr, false, installDir2.string(),
            "file:///nonexistent/should_not_be_fetched_player.exe", robloxStudioUrl);

        assert(outcome.ok);
        std::ifstream in(playerCached, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "already cached, must survive");
    }

    // A Roblox CDN download failure is best-effort -- it must not fail the
    // whole TuxBlox install (the launcher's own lazy-download fallback
    // covers it later, when the user actually launches).
    {
        fs::path installDir3 = work / "install_roblox_fetch_failed";
        auto outcome = runInstall(manifest, [](Step, double) {}, nullptr, false, installDir3.string(),
            "file:///nonexistent/should_fail_but_not_abort_install.exe", robloxStudioUrl);

        assert(outcome.ok);
        assert(!outcome.cancelled);
        assert(fs::exists(installDir3 / "TuxBloxLauncher")); // rest of the install still completed
        assert(!fs::exists(installDir3 / "RobloxPlayer" / "RobloxPlayerInstaller.exe"));
    }

    // Checksum-mismatch path.
    {
        fs::path installDir4 = work / "install_bad_checksum";
        Manifest badManifest = manifest;
        badManifest.protonbuild.sha256 =
            "0000000000000000000000000000000000000000000000000000000000000";
        auto badOutcome = runInstall(badManifest, [](Step, double) {}, nullptr, false, installDir4.string(),
            robloxPlayerUrl, robloxStudioUrl);
        assert(!badOutcome.ok);
        assert(!badOutcome.cancelled);
        assert(badOutcome.errorMessage.find("checksum mismatch") != std::string::npos);
    }

    // Extraction-failure path: checksum-valid but corrupt (non-tar.zst)
    // artifact. Downloads successfully, passes checksum verification, then
    // throws inside extractTarZst -- exercising the catch-all handler. The
    // partially-downloaded .protonbuild.tar.zst.part temp file must be
    // cleaned up rather than left orphaned in the install directory.
    {
        fs::path corruptSrc = work / "corrupt_protonbuild.tar.zst";
        {
            std::ofstream out(corruptSrc, std::ios::binary);
            out << "this is not a valid zstd/tar archive";
        }

        fs::path installDir5 = work / "install_extract_failure";
        Manifest corruptManifest = manifest;
        corruptManifest.protonbuild.url = "file://" + corruptSrc.string();
        corruptManifest.protonbuild.sha256 = sha256File(corruptSrc.string());
        corruptManifest.protonbuild.sizeBytes = fs::file_size(corruptSrc);

        auto corruptOutcome = runInstall(corruptManifest, [](Step, double) {}, nullptr, false, installDir5.string(),
            robloxPlayerUrl, robloxStudioUrl);
        assert(!corruptOutcome.ok);
        assert(!corruptOutcome.cancelled);

        fs::path leftoverTarPart = installDir5 / ".protonbuild.tar.zst.part";
        assert(!fs::exists(leftoverTarPart));
    }

    // Upgrade path: an existing install (simulated: pre-populate the same
    // directory structure a prior install would have left) gets its
    // ProtonBuild/ replaced and its launcher/installer binaries replaced,
    // while runtime/ and other pre-existing content survive untouched.
    {
        fs::path installDir6 = work / "install_upgrade";
        fs::create_directories(installDir6 / "steamapps");
        fs::create_directories(installDir6 / "runtime");
        fs::create_directories(installDir6 / "ProtonBuild" / "dist");
        {
            std::ofstream out(installDir6 / "ProtonBuild" / "dist" / "proton");
            out << "old proton binary";
        }
        {
            std::ofstream out(installDir6 / "ProtonBuild" / "stale_file_removed_in_new_build.txt");
            out << "should be gone after upgrade";
        }
        fs::path runtimeMarker = installDir6 / "runtime" / "session_cookie.txt";
        { std::ofstream out(runtimeMarker); out << "must survive the upgrade"; }

        auto upgradeOutcome = runInstall(manifest, [](Step, double) {}, nullptr, /*isUpgrade=*/true, installDir6.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(upgradeOutcome.ok);
        assert(!upgradeOutcome.cancelled);
        assert(fs::exists(runtimeMarker));                                    // runtime/ untouched
        assert(fs::exists(installDir6 / "ProtonBuild" / "dist" / "proton"));  // new ProtonBuild present
        assert(!fs::exists(installDir6 / "ProtonBuild" / "stale_file_removed_in_new_build.txt")); // old ProtonBuild wiped
        assert(fs::exists(installDir6 / "TuxBloxLauncher"));
        assert(fs::exists(installDir6 / "TuxBloxInstaller"));
    }

    // Upgrade + checksum-mismatch path: a failed upgrade must not touch
    // the pre-existing ProtonBuild/ (never deleted until the replacement is
    // verified) or anything else in the directory.
    {
        fs::path installDir7 = work / "install_upgrade_bad_checksum";
        fs::create_directories(installDir7 / "ProtonBuild" / "dist");
        fs::path survivorMarker = installDir7 / "ProtonBuild" / "dist" / "proton";
        { std::ofstream out(survivorMarker); out << "must survive a failed upgrade"; }

        Manifest badUpgradeManifest = manifest;
        badUpgradeManifest.protonbuild.sha256 =
            "0000000000000000000000000000000000000000000000000000000000000";
        auto badUpgradeOutcome = runInstall(badUpgradeManifest, [](Step, double) {}, nullptr, true, installDir7.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(!badUpgradeOutcome.ok);
        assert(!badUpgradeOutcome.cancelled);
        assert(fs::exists(survivorMarker));
        assert(fs::exists(installDir7)); // the directory itself must never be wiped on an upgrade failure
    }

    fs::remove_all(work);

    printf("installer_steps: all tests passed\n");
    return 0;
}
