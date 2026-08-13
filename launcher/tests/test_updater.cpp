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
    assert(versionNeedsUpdate("0.2.0", "0.1.0") == true); // no downgrade protection

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

    // --- Up-to-date path: both launcher and Proton versions match
    // requiredVersion, no installer fetch happens, needsHandoff stays false. ---
    {
        fs::path installDirPath = work / "install_uptodate";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        writeManifest("ch-uptodate", "0.1.0", installerFileUrl, installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0", fileBaseUrl, "ch-uptodate", "0.1.0",
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(!result.needsHandoff);
        assert(!phases.empty());
        assert(phases.back() == UpdatePhase::UpToDate);
        assert(!fs::exists(installDirPath / "TuxBloxInstaller")); // never fetched -- nothing needed it
    }

    // --- Proton out of date (launcher itself already current): installer
    // gets fetched, verified, and handed off; the launcher itself does no
    // Proton downloading/extracting. ---
    {
        fs::path installDirPath = work / "install_proton_stale";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        writeManifest("ch-protonstale", "0.2.0", installerFileUrl, installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.2.0", fileBaseUrl, "ch-protonstale", "0.2.0",
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
        assert(fs::exists(installDirPath / "ProtonBuild" / "dist" / "version"));
    }

    // --- Launcher itself out of date (Proton already current): same
    // handoff path. ---
    {
        fs::path installDirPath = work / "install_launcher_stale";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.2.0"; }

        writeManifest("ch-launcherstale", "0.2.0", installerFileUrl, installerSha);

        auto result = runUpdateCheck("0.1.0", fileBaseUrl, "ch-launcherstale", "0.2.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(fs::exists(result.installerPath));
    }

    // --- Installer already present and matching the manifest checksum:
    // must not be re-downloaded (its mtime/content stays exactly as-is). ---
    {
        fs::path installDirPath = work / "install_installer_cached";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }
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

        auto result = runUpdateCheck("0.1.0", fileBaseUrl, "ch-installercached", "0.2.0",
            [](UpdateProgress) {}, nullptr, installDirPath.string());

        assert(result.needsHandoff);
        assert(result.installerPath == cachedInstaller.string());
    }

    // --- Checksum mismatch on the freshly-downloaded installer: Error
    // phase, no handoff, no leftover .new temp file. ---
    {
        fs::path installDirPath = work / "install_bad_checksum";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        writeManifest("ch-badchecksum", "0.2.0", installerFileUrl,
            "0000000000000000000000000000000000000000000000000000000000000");

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0", fileBaseUrl, "ch-badchecksum", "0.2.0",
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(!result.needsHandoff);
        assert(!phases.empty());
        assert(phases.back() == UpdatePhase::Error);
        assert(!fs::exists(installDirPath / "TuxBloxInstaller"));
        assert(!fs::exists(installDirPath / "TuxBloxInstaller.new"));
    }

    fs::remove_all(work);

    printf("updater: all tests passed\n");
    return 0;
}
