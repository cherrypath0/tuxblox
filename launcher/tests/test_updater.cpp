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

#include "updater.h"
#include "checksum.h"
#include "install_paths.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    assert(versionNeedsUpdate("0.1.0", "0.1.0") == false);
    assert(versionNeedsUpdate("0.1.0", "0.2.0") == true);
    assert(versionNeedsUpdate("0.2.0", "0.1.0") == false); // never "update" to an older/equal version
    assert(versionNeedsUpdate("1.0.0", "0.2.0") == false); // e.g. installed 1.0.0, manifest still serving 0.2.0
    assert(versionNeedsUpdate("1.0", "1.0.0") == false);   // differing component counts, same value
    assert(versionNeedsUpdate("1.9.0", "1.10.0") == true); // numeric, not lexicographic, comparison

    // downloadProgressFraction: the manifest-size fallback this task adds.
    // Deterministic and independent of any real download/curl timing --
    // see updater.h's doc comment for why this is exposed.
    assert(downloadProgressFraction(50, 100, 999) == 0.5);   // total known -- ignores manifestSize entirely
    assert(downloadProgressFraction(50, 0, 200) == 0.25);    // total unknown -- falls back to manifestSize
    assert(downloadProgressFraction(0, 0, 0) == 0.0);        // neither known -- 0.0, not a divide-by-zero
    assert(downloadProgressFraction(0, 100, 999) == 0.0);    // total known, now=0 -- 0.0 fraction, not the fallback

    fs::path work = fs::temp_directory_path() / "tuxblox_test_updater";
    fs::remove_all(work);
    fs::create_directories(work);

    fs::path installerSrc = work / "TuxBloxInstaller_new_src";
    { std::ofstream out(installerSrc, std::ios::binary); out << "new installer binary"; }
    const std::string installerSha = sha256File(installerSrc.string());
    const uint64_t installerSize = fs::file_size(installerSrc);

    const std::string fileBaseUrl = "file://" + work.string();

    // Writes a manifest fixture at <work>/v1/<channel>/<version>/manifest.json
    // -- runUpdateCheck constructs exactly this path from (baseUrl, channel,
    // requiredVersion), so exercising it through the real path-construction
    // logic (not a flat file it can be pointed at directly) is deliberate.
    auto writeManifest = [&](const std::string& channel, const std::string& version,
                              const std::string& installerUrl, const std::string& installerSha256) {
        fs::path dir = work / "v1" / channel / version;
        fs::create_directories(dir);
        std::ostringstream body;
        body << "{\n"
                "  \"channel\": \"" << channel << "\",\n"
                "  \"manifest_version\": 2,\n"
                "  \"artifacts\": {\n"
                "    \"proton\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size\": 1},\n"
                "    \"launcher\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size\": 1},\n"
                "    \"installer\": {\"url\": \"" << installerUrl << "\", "
                "\"sha256\": \"" << installerSha256 << "\", \"size\": " << installerSize << "}\n"
                "  }\n"
                "}\n";
        std::ofstream out(dir / "manifest.json");
        out << body.str();
    };
    const std::string installerFileUrl = "file://" + installerSrc.string();

    // Fakes an installed compatibility layer: an executable proton/main whose
    // --version output is `buildId` (readBinaryVersion execs it). Every TuxBlox
    // binary answers "x.y.z-channel", which is what the check compares.
    auto writeVersionStub = [](const fs::path& path, const std::string& buildId) {
        fs::create_directories(path.parent_path());
        { std::ofstream out(path); out << "#!/bin/sh\necho " << buildId << "\n"; }
        fs::permissions(path, fs::perms::owner_all);
    };
    auto writeProtonMain = [&](const fs::path& installDirPath, const std::string& buildId) {
        writeVersionStub(installDirPath / "proton" / "main", buildId);
    };
    // A whole install: the layer plus the three binaries beside it. An absent
    // one counts as a mismatch once the layer is there, so a case that means
    // to test something else has to write all of them.
    auto writeWholeInstall = [&](const fs::path& installDirPath, const std::string& buildId) {
        writeProtonMain(installDirPath, buildId);
        writeVersionStub(installDirPath / "TuxBloxInstaller", buildId);
        writeVersionStub(installDirPath / "TuxBloxBootstrapper", buildId);
        writeVersionStub(installDirPath / "studio-mcp", buildId);
    };

    // --- Up-to-date path: both launcher and Proton versions match
    // requiredVersion, no installer fetch happens, needsHandoff stays false. ---
    {
        fs::path installDirPath = work / "install_uptodate";
        writeWholeInstall(installDirPath, "0.1.0-ch-uptodate");

        writeManifest("ch-uptodate", "0.1.0", installerFileUrl, installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0-ch-uptodate", fileBaseUrl, "ch-uptodate", "0.1.0",
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(!result.needsHandoff);
        assert(!phases.empty());
        assert(phases.back() == UpdatePhase::UpToDate);
        // Never fetched -- nothing needed it. The stub written above is still
        // exactly what is on disk, rather than the manifest's installer.
        std::ifstream untouched(installDirPath / "TuxBloxInstaller");
        std::string content((std::istreambuf_iterator<char>(untouched)), std::istreambuf_iterator<char>());
        assert(content.find("new installer binary") == std::string::npos);
    }

    // --- Compatibility layer a build behind the launcher: a half-applied
    // update, so it is reported as mixed and the installer gets fetched,
    // verified and handed off. The launcher itself does no downloading or
    // extracting of the layer. ---
    {
        fs::path installDirPath = work / "install_proton_stale";
        writeWholeInstall(installDirPath, "0.2.0-ch-protonstale");
        writeProtonMain(installDirPath, "0.1.0-ch-protonstale");

        writeManifest("ch-protonstale", "0.2.0", installerFileUrl, installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.2.0-ch-protonstale", fileBaseUrl, "ch-protonstale", "0.2.0",
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.installerPath == (installDirPath / "TuxBloxInstaller").string());
        assert(fs::exists(result.installerPath));
        std::ifstream in(result.installerPath, std::ios::binary);
        std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(content == "new installer binary");
        assert(!phases.empty());
        assert(phases.back() != UpdatePhase::Error);
        // The launcher must never touch Proton itself -- that's the
        // installer's job once handed off to.
        assert(fs::exists(installDirPath / "proton" / "main"));
        // An outdated-but-present layer is not the same as no install at
        // all -- App::updateCheckThreadMain() treats the two differently.
        assert(!result.protonMissing);
        // The pieces disagree, so the update applies without consulting the
        // Auto-Update setting.
        assert(result.mixedInstall);
    }

    // --- No Proton install recorded at all (first run, or a launcher
    // binary run standalone without ever going through the installer):
    // needsHandoff is true same as an outdated install, but protonMissing
    // distinguishes it so App::updateCheckThreadMain() can hand off
    // immediately regardless of the Auto-Update setting -- nothing can be
    // launched yet, so there's nothing to opt out of. ---
    {
        fs::path installDirPath = work / "install_proton_missing";
        // Deliberately no writeProtonMain() call -- installDirPath/proton
        // never gets created. The real launcher always creates installDir()
        // itself before App/runUpdateCheck ever runs (see main.cpp), so
        // this directory-creation call stands in for that, not for
        // anything Proton-related.
        fs::create_directories(installDirPath);

        writeManifest("ch-protonmissing", "0.1.0", installerFileUrl, installerSha);

        auto result = runUpdateCheck("0.1.0-ch-protonmissing", fileBaseUrl, "ch-protonmissing", "0.1.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.protonMissing);
        // Absent is not mismatched: there is no disagreement to report when
        // nothing is installed yet.
        assert(!result.mixedInstall);
        assert(fs::exists(result.installerPath));
    }

    // --- A new release is out and this install is consistently on the old
    // one: the ordinary update. needsHandoff, but NOT mixed -- so the
    // Auto-Update setting still decides whether it is applied now. ---
    {
        fs::path installDirPath = work / "install_launcher_stale";
        writeWholeInstall(installDirPath, "0.1.0-ch-launcherstale");

        writeManifest("ch-launcherstale", "0.2.0", installerFileUrl, installerSha);

        auto result = runUpdateCheck("0.1.0-ch-launcherstale", fileBaseUrl, "ch-launcherstale", "0.2.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(fs::exists(result.installerPath));
        assert(!result.protonMissing);
        assert(!result.mixedInstall);
    }

    // --- Nothing new published, but the install disagrees with itself: the
    // layer is on another channel's build of the same number. Caught, and
    // forced, even though there is no newer release to move to. ---
    {
        fs::path installDirPath = work / "install_mixed_channel";
        writeWholeInstall(installDirPath, "0.1.0-ch-mixed");
        writeProtonMain(installDirPath, "0.1.0-other");

        writeManifest("ch-mixed", "0.1.0", installerFileUrl, installerSha);

        auto result = runUpdateCheck("0.1.0-ch-mixed", fileBaseUrl, "ch-mixed", "0.1.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.mixedInstall);
        assert(fs::exists(result.installerPath));
    }

    // --- A binary is simply gone from an otherwise-current install. Nothing
    // is out of date and nothing disagrees, but the install is incomplete,
    // which is the same kind of broken and gets the same forced repair. ---
    {
        fs::path installDirPath = work / "install_missing_piece";
        writeWholeInstall(installDirPath, "0.1.0-ch-missing");
        fs::remove(installDirPath / "studio-mcp");

        writeManifest("ch-missing", "0.1.0", installerFileUrl, installerSha);

        auto result = runUpdateCheck("0.1.0-ch-missing", fileBaseUrl, "ch-missing", "0.1.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.mixedInstall);
        assert(!result.protonMissing);
    }

    // --- Installer already present and matching the manifest checksum:
    // must not be re-downloaded (its mtime/content stays exactly as-is). ---
    {
        fs::path installDirPath = work / "install_installer_cached";
        writeWholeInstall(installDirPath, "0.1.0-ch-installercached");
        fs::path cachedInstaller = installDirPath / "TuxBloxInstaller";
        { std::ofstream out(cachedInstaller, std::ios::binary); out << "new installer binary"; }
        // Sanity: the pre-placed file's checksum already matches the
        // manifest (both are "new installer binary"), so a correct
        // implementation should skip the download entirely.
        assert(sha256File(cachedInstaller.string()) == installerSha);

        // Point the manifest's installer URL at a nonexistent file -- if the
        // implementation incorrectly tries to re-fetch, this would fail the
        // whole update check instead of silently succeeding.
        writeManifest("ch-installercached", "0.2.0", "file:///nonexistent/should_not_be_fetched", installerSha);

        auto result = runUpdateCheck("0.1.0-ch-installercached", fileBaseUrl, "ch-installercached", "0.2.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.installerPath == cachedInstaller.string());
    }

    // --- Checksum mismatch on the freshly-downloaded installer: Error
    // phase, no handoff, no leftover .new temp file. ---
    {
        fs::path installDirPath = work / "install_bad_checksum";
        writeWholeInstall(installDirPath, "0.1.0-ch-badchecksum");

        writeManifest("ch-badchecksum", "0.2.0", installerFileUrl,
            "0000000000000000000000000000000000000000000000000000000000000");

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0-ch-badchecksum", fileBaseUrl, "ch-badchecksum", "0.2.0",
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(!result.needsHandoff);
        assert(!phases.empty());
        assert(phases.back() == UpdatePhase::Error);
        // A download that failed its checksum is never installed over the copy
        // already there, and leaves no half-written temp file behind.
        std::ifstream kept(installDirPath / "TuxBloxInstaller");
        std::string content((std::istreambuf_iterator<char>(kept)), std::istreambuf_iterator<char>());
        assert(content.find("new installer binary") == std::string::npos);
        assert(!fs::exists(installDirPath / "TuxBloxInstaller.new"));
    }

    fs::remove_all(work);

    printf("updater: all tests passed\n");
    return 0;
}
