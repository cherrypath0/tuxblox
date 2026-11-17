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

#include "bootstrap/install_plan.h"
#include "install_concurrency.h"
#include "roblox_deploy.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;
    using namespace tuxblox::bootstrap;

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
        const auto jobs = buildJobs(packages, LaunchTarget::Studio, "/stage");
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
        assert(std::string(versionExecutable(LaunchTarget::Player)) == "RobloxPlayerBeta.exe");
        assert(std::string(versionExecutable(LaunchTarget::Studio)) == "RobloxStudioBeta.exe");

        const std::string dir = "/tmp/tuxblox-install-plan-test";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        assert(!versionIsInstalled(dir, LaunchTarget::Studio));

        std::ofstream(dir + "/RobloxStudioBeta.exe").put('x');
        assert(versionIsInstalled(dir, LaunchTarget::Studio));
        // The other app's executable is not this app's install.
        assert(!versionIsInstalled(dir, LaunchTarget::Player));

        std::filesystem::remove_all(dir);
        assert(!versionIsInstalled(dir, LaunchTarget::Studio));
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
        assert(!versionIsInstalled(dir, LaunchTarget::Studio));

        std::filesystem::remove_all(dir);
    }

    // Seven packages land in the version root and must serialise against each
    // other; everything else owns its own directory and may extract at once.
    {
        std::vector<std::string> dests;
        for (const char *name : {"RobloxApp.zip", "redist.zip", "shaders.zip", "ssl.zip"}) {
            const auto subdir = packageInstallSubdir(name, LaunchTarget::Player);
            assert(subdir.has_value());
            dests.push_back(packageDestDir("/stage", *subdir));
        }

        const std::vector<std::string> distinct = distinctDestDirs(dests);
        assert(distinct.size() == 3); // the version root, shaders/, ssl/

        // The two root-bound packages share a lock; the others do not.
        assert(destDirIndex(distinct, dests[0]) == destDirIndex(distinct, dests[1]));
        assert(destDirIndex(distinct, dests[2]) != destDirIndex(distinct, dests[0]));
        assert(destDirIndex(distinct, dests[3]) != destDirIndex(distinct, dests[2]));
    }

    // Order is the order first seen, so a lock index is stable across a run.
    {
        const std::vector<std::string> dests = {"/stage/b", "/stage/a", "/stage/b", "/stage/c"};
        const std::vector<std::string> distinct = distinctDestDirs(dests);
        assert(distinct.size() == 3);
        assert(distinct[0] == "/stage/b");
        assert(distinct[1] == "/stage/a");
        assert(destDirIndex(distinct, "/stage/b") == 0);
        assert(destDirIndex(distinct, "/stage/c") == 2);
    }

    // An empty plan needs no locks at all.
    {
        assert(distinctDestDirs({}).empty());
    }

    // With the two halves overlapping the measure must never retreat, or the bar
    // goes backwards and reads as a failure.
    {
        double last = 0.0;
        for (uint64_t bytes = 0; bytes <= 1000; bytes += 100) {
            const double fraction = installFraction(bytes, 1000, 0, 10);
            assert(fraction >= last);
            last = fraction;
        }
        for (size_t done = 0; done <= 10; ++done) {
            const double fraction = installFraction(1000, 1000, done, 10);
            assert(fraction >= last);
            last = fraction;
        }
        assert(last <= 1.0);
    }

    // Downloading everything is not finished -- extraction is still to come.
    {
        assert(installFraction(1000, 1000, 0, 10) < 1.0);
        assert(installFraction(1000, 1000, 10, 10) == 1.0);
        assert(installFraction(0, 1000, 0, 10) == 0.0);
    }

    // A plan with nothing to weigh must not divide by zero.
    {
        assert(installFraction(0, 0, 0, 0) == 1.0);
    }

    // A mirror retry restarts that package's byte count at zero, so the raw measure DOES fall.
    // This is the case the published value has to absorb.
    {
        assert(installFraction(400, 1000, 0, 10) > installFraction(0, 1000, 0, 10));
    }

    // The ratchet is what a progress bar is given, and it never retreats.
    {
        double highest = 0.0;
        const double climbed = monotonicInstallFraction(highest, 400, 1000, 0, 10);
        // 400 MB in on the first mirror, that mirror dies, the second starts from nothing.
        const double afterRetry = monotonicInstallFraction(highest, 0, 1000, 0, 10);
        assert(afterRetry == climbed);
        // And a later sample that genuinely is higher still moves it on.
        assert(monotonicInstallFraction(highest, 1000, 1000, 0, 10) > climbed);
    }
    // Interleaving the two halves out of order cannot drop it either.
    {
        double highest = 0.0;
        monotonicInstallFraction(highest, 1000, 1000, 3, 10);
        const double peak = highest;
        assert(monotonicInstallFraction(highest, 900, 1000, 0, 10) == peak);
    }

    printf("install_plan: all tests passed\n");
    return 0;
}
