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
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

static void makeFixtureTarZst(const std::string& path, const char* fileContent) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    struct archive_entry* entry = archive_entry_new();
    // Contents at the archive root, per the packing convention in
    // installer_steps.cpp -- the target dir comes from the manifest filename.
    archive_entry_set_pathname(entry, "main");
    archive_entry_set_size(entry, static_cast<int64_t>(strlen(fileContent)));
    archive_entry_set_filetype(entry, AE_IFREG);
    archive_entry_set_perm(entry, 0755);
    archive_write_header(a, entry);
    archive_write_data(a, fileContent, strlen(fileContent));
    archive_entry_free(entry);

    archive_write_close(a);
    archive_write_free(a);
}

// Same as above but writes several entries, including one inside a
// subdirectory -- used to fake a multi-file, archive-shaped artifact
// (TuxBloxLauncher + lib/ + plugins/ + qt.conf, all at the archive root, per
// the same packing convention). `execEntry` gets 0755, the rest 0644, so the
// extracted executable's mode can be asserted.
static void makeFixtureBundleTarZst(const std::string& path,
                                     const std::vector<std::pair<std::string, std::string>>& entries,
                                     const std::string& execEntry) {
    struct archive* a = archive_write_new();
    archive_write_add_filter_zstd(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_filename(a, path.c_str());

    for (const auto& [name, content] : entries) {
        struct archive_entry* entry = archive_entry_new();
        archive_entry_set_pathname(entry, name.c_str());
        archive_entry_set_size(entry, static_cast<int64_t>(content.size()));
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, name == execEntry ? 0755 : 0644);
        archive_write_header(a, entry);
        archive_write_data(a, content.data(), content.size());
        archive_entry_free(entry);
    }

    archive_write_close(a);
    archive_write_free(a);
}

int main() {
    using namespace tuxblox;

    // downloadProgressFraction: the manifest-size fallback this task adds.
    // Deterministic and independent of any real download/curl timing --
    // see installer_steps.h's doc comment for why this is exposed.
    assert(downloadProgressFraction(50, 100, 999) == 0.5);   // total known -- ignores manifestSize entirely
    assert(downloadProgressFraction(50, 0, 200) == 0.25);    // total unknown -- falls back to manifestSize
    assert(downloadProgressFraction(0, 0, 0) == 0.0);        // neither known -- 0.0, not a divide-by-zero
    assert(downloadProgressFraction(0, 100, 999) == 0.0);    // total known, now=0 -- 0.0 fraction, not the fallback

    fs::path work = fs::temp_directory_path() / "tuxblox_test_installer_steps";
    fs::remove_all(work);
    fs::create_directories(work);

    fs::path tarballSrc = work / "protonbuild.tar.zst";
    fs::path launcherSrc = work / "TuxBloxLauncherSrc";
    fs::path installerSrc = work / "TuxBloxInstallerSrc";
    fs::path mcpSrc = work / "McpSrc";
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
        std::ofstream out(mcpSrc, std::ios::binary);
        out << "fake studio-mcp contents";
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

    // A published tree, exactly as the CDN serves one:
    //   client/components/<component>/<version>/x86_64/<published name>
    // runInstall builds every URL from the release and the layout table, so an artifact cannot be
    // pointed at an arbitrary file any more -- which means this exercises the real path
    // construction rather than stepping around it.
    const std::string releaseBaseUrl = "file://" + work.string();
    const std::map<std::string, std::string> kVersions = {
        {"compat", "3.3.0"}, {"launcher", "3.5.0"}, {"installer", "3.1.0"},
        {"studio-mcp", "2.0.1"}, {"libtuxblox", "3.1.0"},
    };

    auto publishedPath = [&](const std::string& name) {
        return work / "client" / "components" / name / kVersions.at(name) / "x86_64" /
               componentLayout(name)->publishedAs;
    };
    auto publishFrom = [&](const std::string& name, const fs::path& src) {
        const fs::path dest = publishedPath(name);
        fs::create_directories(dest.parent_path());
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
    };

    publishFrom("compat", tarballSrc);
    publishFrom("launcher", launcherSrc);
    publishFrom("installer", installerSrc);
    publishFrom("studio-mcp", mcpSrc);
    fs::create_directories(publishedPath("libtuxblox").parent_path());
    makeFixtureTarZst(publishedPath("libtuxblox").string(), "fake libtuxblox dist");

    // Publishes `src` as a NEW version of `name` and points `r` at it, leaving every other
    // release's bytes alone.
    auto republish = [&](ReleaseFile& r, const std::string& name, const std::string& version,
                          const fs::path& src) {
        const fs::path dest = work / "client" / "components" / name / version / "x86_64" /
                               componentLayout(name)->publishedAs;
        fs::create_directories(dest.parent_path());
        fs::copy_file(src, dest, fs::copy_options::overwrite_existing);
        r.components[name].version = version;
        r.components[name].x86_64 = ComponentBytes{sha256File(dest.string()), fs::file_size(dest)};
    };

    auto buildRelease = [&]() {
        ReleaseFile r;
        r.release = "3.6.0.0";
        r.channel = "canary";
        r.changelog = "components";
        for (const auto& [name, version] : kVersions) {
            ReleaseComponent c;
            c.version = version;
            const fs::path file = publishedPath(name);
            c.x86_64 = ComponentBytes{sha256File(file.string()), fs::file_size(file)};
            r.components[name] = c;
        }
        return r;
    };
    const ReleaseFile release = buildRelease();
    const std::string releaseJson = R"({"release":"3.6.0.0","channel":"canary"})";

    // Happy path: fresh install. Also pre-warms the Roblox Player/Studio
    // installer cache -- never runs them, only downloads and caches them
    // at the exact paths the launcher's own lazy-download fallback checks.
    {
        std::string lastLabel;
        double lastPercent = -1.0;
        auto outcome = runInstall(release, releaseBaseUrl,
            [&](const std::string& label, double percent) { lastLabel = label; lastPercent = percent; },
            nullptr, /*isUpgrade=*/false, installDirPath.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!outcome.cancelled);
        assert(lastLabel == "Downloading Roblox Studio"); // chronologically the final phase
        assert(lastPercent > 99.9);
        assert(fs::exists(installDirPath / "runtime"));
        assert(fs::exists(installDirPath / "compat" / "main"));
        assert(fs::exists(installDirPath / "TuxBloxLauncher"));
        assert(fs::exists(installDirPath / "TuxBloxInstaller"));
        // The interface libraries, which are an archive like the layer is.
        assert(fs::exists(installDirPath / "libtuxblox"));
        // Studio MCP ships exactly as stage_release() publishes it: a flat
        // file at the install root. It must arrive executable, since it is run
        // as a program rather than read.
        assert(fs::exists(installDirPath / "studio-mcp"));
        assert((fs::status(installDirPath / "studio-mcp").permissions() &
                fs::perms::owner_exec) != fs::perms::none);

        // The launcher's resolved install path comes back from the
        // pipeline itself, not assumed by the caller.
        assert(outcome.launcherPath == (installDirPath / "TuxBloxLauncher").string());

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

    // Archive-shaped launcher artifact: a "launcher" artifact whose URL ends
    // An archive component lands as a FOLDER named by its layout entry, with the archive root's
    // entries directly inside it -- not nested one extra level. Whether a component is an archive
    // comes from the layout table now rather than from its URL's extension, so the two that are
    // archives (the layer and the libraries) are where this shape lives.
    {
        fs::path stackSrc = work / "libtuxblox_bundle.tar.zst";
        makeFixtureBundleTarZst(stackSrc.string(), {
            {"lib/libgtk-4.so", "fake gtk"},
            {"share/icons/TuxBlox/index.theme", "[Icon Theme]\n"},
            {"VERSION", "3.1.0\n"},
        }, "lib/libgtk-4.so");

        fs::path installDir8 = work / "install_stack_bundle";
        ReleaseFile bundled = release;
        republish(bundled, "libtuxblox", "9.0.0", stackSrc);

        auto outcome = runInstall(bundled, releaseBaseUrl, [](const std::string&, double) {}, nullptr,
            false, installDir8.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!outcome.cancelled);
        assert(fs::exists(installDir8 / "libtuxblox" / "lib" / "libgtk-4.so"));
        assert(fs::exists(installDir8 / "libtuxblox" / "VERSION"));
        // A tarball packed with a top-level wrapper directory would produce
        // installDir/libtuxblox/libtuxblox/..., which is what the convention exists to prevent.
        assert(!fs::exists(installDir8 / "libtuxblox" / "libtuxblox"));
        // The launcher is a flat file and still the thing handed back to exec.
        assert(outcome.launcherPath == (installDir8 / "TuxBloxLauncher").string());
        assert(fs::is_regular_file(outcome.launcherPath));
    }

    // The packing convention, enforced: a launcher bundle packed WITH a
    // top-level wrapper directory (the exact mistake installer_steps.cpp's
    // PACKING CONVENTION comment warns about) must fail the install loudly
    // rather than "succeed" with a launcherPath pointing at nothing.
    {
        fs::path badBundleSrc = work / "libtuxblox_wrapped.tar.zst";
        makeFixtureBundleTarZst(badBundleSrc.string(), {
            {"libtuxblox/lib/libgtk-4.so", "wrongly nested library"},
            {"libtuxblox/VERSION", "3.1.0\n"},
        }, "libtuxblox/lib/libgtk-4.so");

        fs::path installDirWrapped = work / "install_stack_wrapped";
        ReleaseFile wrapped = release;
        republish(wrapped, "libtuxblox", "9.0.1", badBundleSrc);

        auto outcome = runInstall(wrapped, releaseBaseUrl, [](const std::string&, double) {}, nullptr,
            false, installDirWrapped.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(!outcome.ok);
        assert(!outcome.cancelled);
        assert(outcome.errorMessage.find("wrapper directory") != std::string::npos);
        // The double nesting the convention exists to prevent, for the record.
        assert(fs::exists(installDirWrapped / "libtuxblox" / "libtuxblox"));
    }

    // A manifest with no "launcher" artifact fails fast, before any
    // downloading -- there'd be nothing to hand back for the caller to
    // exec, so this is the one case installer_steps.cpp still hardcodes a
    // check for.
    {
        ReleaseFile noLauncher = release;
        noLauncher.components.erase("launcher");
        fs::path installDirNoLauncher = work / "install_no_launcher";
        auto outcome = runInstall(noLauncher, releaseBaseUrl, [](const std::string&, double) {},
            nullptr, false, installDirNoLauncher.string(), robloxPlayerUrl, robloxStudioUrl);
        assert(!outcome.ok);
        assert(!outcome.cancelled);
        assert(outcome.errorMessage.find("launcher") != std::string::npos);
        assert(!fs::exists(installDirNoLauncher / "runtime")); // failed before doing anything
    }

    // Extraction progress: the Proton phase must report intermediate
    // fractions across download+verify+extract, not just jump from 0 to
    // 100 -- guards against the "Downloading/Upgrading Proton" bar
    // freezing for the whole step.
    {
        fs::path installDir9 = work / "install_extract_progress";
        std::vector<double> protonPercents;
        auto outcome = runInstall(release, releaseBaseUrl,
            [&](const std::string& label, double percent) {
                if (label == "Downloading Compatibility layer") protonPercents.push_back(percent);
            },
            nullptr, /*isUpgrade=*/false, installDir9.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        // Do NOT weaken this to a smaller bound -- it exists specifically
        // to catch progress reporting collapsing back to a start/end jump.
        assert(protonPercents.size() >= 4);
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

        auto outcome = runInstall(release, releaseBaseUrl, [](const std::string&, double) {}, nullptr, false, installDir2.string(),
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
        auto outcome = runInstall(release, releaseBaseUrl, [](const std::string&, double) {}, nullptr, false, installDir3.string(),
            "file:///nonexistent/should_fail_but_not_abort_install.exe", robloxStudioUrl);

        assert(outcome.ok);
        assert(!outcome.cancelled);
        assert(fs::exists(installDir3 / "TuxBloxLauncher")); // rest of the install still completed
        assert(!fs::exists(installDir3 / "RobloxPlayer" / "RobloxPlayerInstaller.exe"));
    }

    // Checksum-mismatch path.
    {
        fs::path installDir4 = work / "install_bad_checksum";
        ReleaseFile badRelease = release;
        badRelease.components["compat"].x86_64->sha256 =
            "0000000000000000000000000000000000000000000000000000000000000";
        auto badOutcome = runInstall(badRelease, releaseBaseUrl, [](const std::string&, double) {},
            nullptr, false, installDir4.string(), robloxPlayerUrl, robloxStudioUrl);
        assert(!badOutcome.ok);
        assert(!badOutcome.cancelled);
        assert(badOutcome.errorMessage.find("checksum mismatch") != std::string::npos);
    }

    // Extraction-failure path: checksum-valid but corrupt (non-tar.zst)
    // artifact. Downloads successfully, passes checksum verification, then
    // throws inside extractTarZst -- exercising the catch-all handler. The
    // partially-downloaded temp file (named after the component key, e.g.
    // ".compat.tar.part") must be cleaned up rather than left orphaned in
    // the install directory.
    {
        fs::path corruptSrc = work / "corrupt_protonbuild.tar.zst";
        {
            std::ofstream out(corruptSrc, std::ios::binary);
            out << "this is not a valid zstd/tar archive";
        }

        fs::path installDir5 = work / "install_extract_failure";
        ReleaseFile corruptRelease = release;
        republish(corruptRelease, "compat", "9.0.2", corruptSrc);

        auto corruptOutcome = runInstall(corruptRelease, releaseBaseUrl,
            [](const std::string&, double) {}, nullptr, false, installDir5.string(),
            robloxPlayerUrl, robloxStudioUrl);
        assert(!corruptOutcome.ok);
        assert(!corruptOutcome.cancelled);

        fs::path leftoverTarPart = installDir5 / ".compat.tar.part";
        assert(!fs::exists(leftoverTarPart));
    }

    // Upgrade path: an existing install (simulated: pre-populate the same
    // directory structure a prior install would have left) gets its
    // compat/ replaced and its launcher/installer files
    // replaced, while runtime/ and other pre-existing content survive
    // untouched.
    {
        fs::path installDir6 = work / "install_upgrade";
        fs::create_directories(installDir6 / "runtime");
        fs::create_directories(installDir6 / "compat");
        {
            std::ofstream out(installDir6 / "compat" / "main");
            out << "old layer binary";
        }
        {
            std::ofstream out(installDir6 / "compat" / "stale_file_removed_in_new_build.txt");
            out << "should be gone after upgrade";
        }
        fs::path runtimeMarker = installDir6 / "runtime" / "session_cookie.txt";
        { std::ofstream out(runtimeMarker); out << "must survive the upgrade"; }

        auto upgradeOutcome = runInstall(release, releaseBaseUrl, [](const std::string&, double) {}, nullptr, /*isUpgrade=*/true,
            installDir6.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(upgradeOutcome.ok);
        assert(!upgradeOutcome.cancelled);
        assert(fs::exists(runtimeMarker));                          // runtime/ untouched
        assert(fs::exists(installDir6 / "compat" / "main"));        // the new layer is present
        assert(!fs::exists(installDir6 / "compat" / "stale_file_removed_in_new_build.txt")); // the old one is gone
        assert(fs::exists(installDir6 / "TuxBloxLauncher"));
        assert(fs::exists(installDir6 / "TuxBloxInstaller"));
        assert(fs::exists(installDir6 / "studio-mcp"));
        assert(fs::exists(installDir6 / "libtuxblox"));
    }

    // Upgrade + checksum-mismatch path: a failed upgrade must not touch
    // the pre-existing compat/ (never deleted until the replacement is
    // verified) or anything else in the directory.
    {
        fs::path installDir7 = work / "install_upgrade_bad_checksum";
        fs::create_directories(installDir7 / "compat");
        fs::path survivorMarker = installDir7 / "compat" / "main";
        { std::ofstream out(survivorMarker); out << "must survive a failed upgrade"; }

        ReleaseFile badUpgrade = release;
        badUpgrade.components["compat"].x86_64->sha256 =
            "0000000000000000000000000000000000000000000000000000000000000";
        auto badUpgradeOutcome = runInstall(badUpgrade, releaseBaseUrl,
            [](const std::string&, double) {}, nullptr, true, installDir7.string(),
            robloxPlayerUrl, robloxStudioUrl);

        assert(!badUpgradeOutcome.ok);
        assert(!badUpgradeOutcome.cancelled);
        assert(fs::exists(survivorMarker));
        assert(fs::exists(installDir7)); // the directory itself must never be wiped on an upgrade failure
    }

    // The shared libraries are one more component, extracted into libtuxblox/
    fs::path librariesSrc = work / "libtuxblox_new.tar.zst";
    makeFixtureBundleTarZst(librariesSrc.string(), {{"lib/x86_64-linux-gnu/libgtk-4.so.1", "new gtk library"}}, "");
    ReleaseFile withLibraries = release;
    republish(withLibraries, "libtuxblox", "9.0.3", librariesSrc);

    // An install upgraded from a Qt launcher has Qt's libraries in the same folder, and upgrading replaces them as it replaces any other artifact
    {
        fs::path installDir9 = work / "install_upgrade_from_qt";
        fs::create_directories(installDir9 / "libtuxblox" / "lib");
        { std::ofstream out(installDir9 / "libtuxblox" / "lib" / "libQt6Core.so.6"); out << "old qt library"; }
        fs::path runtimeMarker = installDir9 / "runtime" / "session_cookie.txt";
        fs::create_directories(runtimeMarker.parent_path());
        { std::ofstream out(runtimeMarker); out << "must survive the upgrade"; }

        auto outcome = runInstall(withLibraries, releaseBaseUrl, [](const std::string&, double) {},
            nullptr, /*isUpgrade=*/true, installDir9.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!fs::exists(installDir9 / "libtuxblox" / "lib" / "libQt6Core.so.6"));
        assert(fs::exists(installDir9 / "libtuxblox" / "lib" / "x86_64-linux-gnu" / "libgtk-4.so.1"));
        assert(fs::exists(runtimeMarker));
    }

    // A download that fails part-way must leave the old launcher and the old libraries it needs as a working pair
    {
        fs::path installDir10 = work / "install_failed_upgrade_pair";
        fs::create_directories(installDir10 / "libtuxblox" / "lib");
        { std::ofstream out(installDir10 / "libtuxblox" / "lib" / "libQt6Core.so.6"); out << "old qt library"; }
        { std::ofstream out(installDir10 / "TuxBloxLauncher"); out << "old launcher binary"; }

        ReleaseFile badComponent = withLibraries;
        badComponent.components["studio-mcp"].x86_64->sha256 =
            "0000000000000000000000000000000000000000000000000000000000000";
        auto outcome = runInstall(badComponent, releaseBaseUrl, [](const std::string&, double) {},
            nullptr, /*isUpgrade=*/true, installDir10.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(!outcome.ok);
        assert(fs::exists(installDir10 / "libtuxblox" / "lib" / "libQt6Core.so.6"));
        std::ifstream in(installDir10 / "TuxBloxLauncher");
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "old launcher binary");
    }

    // Order: everything else, then the libraries, then the launcher, so the window in which the two do not match is one small download wide
    {
        fs::path installDir12 = work / "install_order";
        std::vector<std::string> labels;
        auto outcome = runInstall(withLibraries, releaseBaseUrl, [&](const std::string& label, double) {
            if (label.rfind("Upgrading ", 0) == 0 && (labels.empty() || labels.back() != label)) labels.push_back(label);
        }, nullptr, /*isUpgrade=*/true, installDir12.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(labels.size() >= 3);
        assert(labels.back() == "Upgrading Launcher");
        assert(labels[labels.size() - 2] == "Upgrading Libraries");
    }

    // A TuxBloxBootstrapper left behind by an install made before the launcher
    // carried the Roblox download itself. No release ships that file any more,
    // so an upgrade has to take it away rather than leave it sitting there.
    {
        fs::path installDir13 = work / "install_leftover_bootstrapper";
        fs::create_directories(installDir13);
        { std::ofstream out(installDir13 / "TuxBloxBootstrapper"); out << "old bootstrapper binary"; }
        assert(fs::exists(installDir13 / "TuxBloxBootstrapper"));

        auto outcome = runInstall(release, releaseBaseUrl, [](const std::string&, double) {}, nullptr,
            /*isUpgrade=*/true, installDir13.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!fs::exists(installDir13 / "TuxBloxBootstrapper"));
        // And the upgrade still did everything else it was there to do.
        assert(fs::exists(installDir13 / "TuxBloxLauncher"));
        assert(fs::exists(installDir13 / "compat" / "main"));
    }

    // Having nothing to remove is the ordinary case and must not be an error.
    {
        fs::path installDir14 = work / "install_no_leftover";
        auto outcome = runInstall(release, releaseBaseUrl, [](const std::string&, double) {}, nullptr,
            /*isUpgrade=*/true, installDir14.string(), robloxPlayerUrl, robloxStudioUrl);

        assert(outcome.ok);
        assert(!fs::exists(installDir14 / "TuxBloxBootstrapper"));
        assert(fs::exists(installDir14 / "TuxBloxLauncher"));
    }

    fs::remove_all(work);

    printf("installer_steps: all tests passed\n");
    return 0;
}
