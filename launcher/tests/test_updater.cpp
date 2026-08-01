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

    fs::path work = fs::temp_directory_path() / "tuxblox_test_updater";
    fs::remove_all(work);
    fs::create_directories(work);

    fs::path installerSrc = work / "TuxBloxInstaller_new_src";
    { std::ofstream out(installerSrc, std::ios::binary); out << "new installer binary"; }
    const std::string installerSha = sha256File(installerSrc.string());
    const uint64_t installerSize = fs::file_size(installerSrc);

    auto writeManifest = [&](const fs::path& path, const std::string& tuxbloxVersion,
                              const std::string& protonVersion, const std::string& installerSha256) {
        std::ostringstream body;
        body << "{\n"
                "  \"manifest_version\": 1,\n"
                "  \"tuxblox_version\": \"" << tuxbloxVersion << "\",\n"
                "  \"proton_version\": \"" << protonVersion << "\",\n"
                "  \"artifacts\": {\n"
                "    \"protonbuild\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size_bytes\": 1},\n"
                "    \"launcher\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size_bytes\": 1},\n"
                "    \"installer\": {\"url\": \"file://" << installerSrc.string() << "\", "
                "\"sha256\": \"" << installerSha256 << "\", \"size_bytes\": " << installerSize << "}\n"
                "  }\n"
                "}\n";
        std::ofstream out(path);
        out << body.str();
    };

    // --- Up-to-date path: both launcher and Proton versions match the
    // manifest, no installer fetch happens, needsHandoff stays false. ---
    {
        fs::path installDirPath = work / "install_uptodate";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        fs::path manifestSrc = work / "manifest_uptodate.json";
        writeManifest(manifestSrc, "0.1.0", "0.1.0", installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0", "file://" + manifestSrc.string(),
            [&](UpdateProgress p) { phases.push_back(p.phase); }, nullptr, installDirPath.string());

        assert(!result.needsHandoff);
        assert(!phases.empty());
        assert(phases.back() == UpdatePhase::UpToDate);
        assert(!fs::exists(installDirPath / "TuxBloxInstaller")); // never fetched -- nothing needed it
    }

    // --- Proton out of date: installer gets fetched, verified, and handed
    // off; the launcher itself does no Proton downloading/extracting. ---
    {
        fs::path installDirPath = work / "install_proton_stale";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        fs::path manifestSrc = work / "manifest_proton_stale.json";
        writeManifest(manifestSrc, "0.1.0", "0.2.0", installerSha);

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0", "file://" + manifestSrc.string(),
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

    // --- Launcher itself out of date: same handoff path. ---
    {
        fs::path installDirPath = work / "install_launcher_stale";
        fs::create_directories(installDirPath / "ProtonBuild" / "dist");
        { std::ofstream out(installDirPath / "ProtonBuild" / "dist" / "version"); out << "1700000000 0.1.0"; }

        fs::path manifestSrc = work / "manifest_launcher_stale.json";
        writeManifest(manifestSrc, "0.2.0", "0.1.0", installerSha);

        auto result = runUpdateCheck("0.1.0", "file://" + manifestSrc.string(),
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

        fs::path manifestSrc = work / "manifest_installer_cached.json";
        writeManifest(manifestSrc, "0.2.0", "0.1.0", installerSha);

        // Point the manifest's installer URL at a nonexistent file -- if the
        // implementation incorrectly tries to re-fetch, this would fail the
        // whole update check instead of silently succeeding.
        std::ostringstream body;
        body << "{\n"
                "  \"manifest_version\": 1,\n"
                "  \"tuxblox_version\": \"0.2.0\",\n"
                "  \"proton_version\": \"0.1.0\",\n"
                "  \"artifacts\": {\n"
                "    \"protonbuild\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size_bytes\": 1},\n"
                "    \"launcher\": {\"url\": \"file:///nonexistent\", \"sha256\": \"x\", \"size_bytes\": 1},\n"
                "    \"installer\": {\"url\": \"file:///nonexistent/should_not_be_fetched\", "
                "\"sha256\": \"" << installerSha << "\", \"size_bytes\": " << installerSize << "}\n"
                "  }\n"
                "}\n";
        { std::ofstream out(manifestSrc); out << body.str(); }

        auto result = runUpdateCheck("0.1.0", "file://" + manifestSrc.string(),
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

        fs::path manifestSrc = work / "manifest_bad_checksum.json";
        writeManifest(manifestSrc, "0.1.0", "0.2.0",
            "0000000000000000000000000000000000000000000000000000000000000");

        std::vector<UpdatePhase> phases;
        auto result = runUpdateCheck("0.1.0", "file://" + manifestSrc.string(),
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
