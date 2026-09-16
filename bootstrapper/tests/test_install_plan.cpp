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

#include "install_plan.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;

    // Unpinned: the named server is tried first, the other mirror second.
    {
        Config c;
        c.server = "setup.rbxcdn.com";
        c.serverPinned = false;
        const auto mirrors = mirrorsFor(c);
        assert(mirrors.size() == 2);
        assert(mirrors[0] == "setup.rbxcdn.com");
        assert(mirrors[1] == "setup-aws.rbxcdn.com");
    }

    // Naming the other mirror still gets a fallback, in the other order.
    {
        Config c;
        c.server = "setup-aws.rbxcdn.com";
        c.serverPinned = false;
        const auto mirrors = mirrorsFor(c);
        assert(mirrors.size() == 2);
        assert(mirrors[0] == "setup-aws.rbxcdn.com");
        assert(mirrors[1] == "setup.rbxcdn.com");
    }

    // Pinned means pinned: a run told to use one mirror must not quietly
    // fetch from another.
    {
        Config c;
        c.server = "setup-aws.rbxcdn.com";
        c.serverPinned = true;
        const auto mirrors = mirrorsFor(c);
        assert(mirrors.size() == 1);
        assert(mirrors[0] == "setup-aws.rbxcdn.com");
    }

    // An unknown host has no sibling to fall back to.
    {
        Config c;
        c.server = "mirror.example.com";
        c.serverPinned = false;
        const auto mirrors = mirrorsFor(c);
        assert(mirrors.size() == 1);
        assert(mirrors[0] == "mirror.example.com");
    }

    // Where a version is installed, and where one package inside it goes.
    assert(versionDir("/versions", "version-abc") == "/versions/version-abc");
    assert(versionDir("/versions/", "version-abc") == "/versions/version-abc");
    assert(packageDestDir("/versions/version-abc", "") == "/versions/version-abc");
    assert(packageDestDir("/versions/version-abc", "shaders/") ==
           "/versions/version-abc/shaders/");

    // Roblox's client reads this to find its content folder; a version
    // without it does not start.
    {
        const std::string xml = appSettingsXml();
        assert(xml.find("<ContentFolder>content</ContentFolder>") != std::string::npos);
        assert(xml.find("<BaseUrl>http://www.roblox.com</BaseUrl>") != std::string::npos);
        assert(xml.rfind("<?xml", 0) == 0);
    }


    // buildJobs(): unknown packages are dropped, everything else gets its
    // download path and its destination inside the staging directory.
    {
        std::vector<PackageEntry> packages = {
            {"RobloxApp.zip", "aa", 10, 20},
            {"shaders.zip", "bb", 30, 40},
            {"SomeFuturePackage.zip", "cc", 50, 60},
            {"WebView2RuntimeInstaller.zip", "dd", 70, 80},
        };
        const auto jobs = buildJobs(packages, RobloxApp::Studio, "/stage");
        assert(jobs.size() == 2);
        assert(jobs[0].name == "RobloxApp.zip");
        assert(jobs[0].archivePath == "/stage/RobloxApp.zip");
        assert(jobs[0].destDir == "/stage");
        assert(jobs[0].md5 == "aa");
        assert(jobs[0].packedSize == 10);
        assert(jobs[1].name == "shaders.zip");
        assert(jobs[1].destDir == "/stage/shaders/");

        // Total packed bytes drives the download progress bar.
        assert(totalPackedBytes(jobs) == 40);
    }


    // A version counts as installed only when its executable is there. The
    // launcher's version list uses the same rule, so a half-finished folder
    // is repaired rather than skipped.
    {
        assert(std::string(versionExecutable(RobloxApp::Player)) == "RobloxPlayerBeta.exe");
        assert(std::string(versionExecutable(RobloxApp::Studio)) == "RobloxStudioBeta.exe");

        const std::string dir = "/tmp/tuxblox-install-plan-test";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        assert(!versionIsInstalled(dir, RobloxApp::Studio));

        std::ofstream(dir + "/RobloxStudioBeta.exe").put('x');
        assert(versionIsInstalled(dir, RobloxApp::Studio));
        // The other app's executable is not this app's install.
        assert(!versionIsInstalled(dir, RobloxApp::Player));

        std::filesystem::remove_all(dir);
        assert(!versionIsInstalled(dir, RobloxApp::Studio));
    }


    // When an exact version is named, the hash alone decides which app it
    // is -- so either executable means it is installed. Without this a
    // pinned Player hash under the default studio setting looks missing
    // forever and re-downloads on every run.
    {
        const std::string dir = "/tmp/tuxblox-install-plan-anyapp";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        assert(!versionIsInstalledForAnyApp(dir));

        std::ofstream(dir + "/RobloxPlayerBeta.exe").put('x');
        assert(versionIsInstalledForAnyApp(dir));
        assert(!versionIsInstalled(dir, RobloxApp::Studio));

        std::filesystem::remove_all(dir);
    }

    printf("install_plan: all tests passed\n");
    return 0;
}
