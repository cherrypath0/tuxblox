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

#include "prefix_session.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Writes a NUL-delimited blob, the shape /proc/<pid>/cmdline and environ use.
static void writeNulJoined(const fs::path& p, const std::vector<std::string>& parts) {
    std::ofstream f(p, std::ios::binary);
    for (const auto& s : parts) {
        f.write(s.data(), static_cast<std::streamsize>(s.size()));
        f.put('\0');
    }
}

static void makeProcEntry(const fs::path& procRoot, const std::string& pid,
                          const std::string& cmdline, const std::string& wineprefix) {
    fs::create_directories(procRoot / pid);
    writeNulJoined(procRoot / pid / "cmdline", {cmdline});
    std::vector<std::string> env = {"PATH=/usr/bin"};
    if (!wineprefix.empty()) env.push_back("WINEPREFIX=" + wineprefix);
    writeNulJoined(procRoot / pid / "environ", env);
}

int main() {
    using namespace tuxblox;

    // Image-name extraction: cut at the first ".exe", not the first space --
    // the image path contains spaces, and later arguments contain further
    // ".exe" paths.
    assert(wineImageNameFromCmdline("C:\\users\\user\\RobloxStudioBeta.exe") ==
           "robloxstudiobeta.exe");
    assert(wineImageNameFromCmdline("C:\\Program Files\\Roblox Studio\\RobloxStudioBeta.exe -foo") ==
           "robloxstudiobeta.exe");
    assert(wineImageNameFromCmdline(
               "C:\\x\\RobloxCrashHandler.exe --attachment=C:\\y\\RobloxStudioBeta.exe") ==
           "robloxcrashhandler.exe");
    assert(wineImageNameFromCmdline("C:/users/user/RobloxPlayerBeta.exe") ==
           "robloxplayerbeta.exe");
    assert(wineImageNameFromCmdline("/usr/bin/bash").empty());
    assert(wineImageNameFromCmdline("").empty());

    const fs::path tmp = fs::temp_directory_path() / "tuxblox_prefix_session_test";
    fs::remove_all(tmp);
    const fs::path procRoot = tmp / "proc";
    fs::create_directories(procRoot);

    const std::string wanted = (tmp / "runtime" / "pfx").string();
    const std::string other  = (tmp / "otherprefix" / "pfx").string();

    // Nothing running at all.
    assert(!prefixHasSessionHolderIn(procRoot.string(), wanted));

    // Non-numeric /proc entries are skipped, not treated as pids.
    fs::create_directories(procRoot / "self");
    assert(!prefixHasSessionHolderIn(procRoot.string(), wanted));

    // A holder image, but in a different prefix.
    makeProcEntry(procRoot, "101", "C:\\x\\RobloxStudioBeta.exe", other);
    assert(!prefixHasSessionHolderIn(procRoot.string(), wanted));

    // A process in the right prefix, but not a holder image.
    makeProcEntry(procRoot, "102", "C:\\windows\\system32\\explorer.exe", wanted);
    assert(!prefixHasSessionHolderIn(procRoot.string(), wanted));

    // The real thing.
    makeProcEntry(procRoot, "103", "C:\\x\\RobloxStudioBeta.exe", wanted);
    assert(prefixHasSessionHolderIn(procRoot.string(), wanted));

    // session.cpp sets WINEPREFIX with a trailing slash and normalises it away on
    // comparison -- both spellings must match.
    assert(prefixHasSessionHolderIn(procRoot.string(), wanted + "/"));
    fs::remove_all(procRoot / "103");
    makeProcEntry(procRoot, "104", "C:\\x\\RobloxStudioBeta.exe", wanted + "/");
    assert(prefixHasSessionHolderIn(procRoot.string(), wanted));

    // The installer counts as a holder too (SESSION_HOLDER_IMAGES).
    fs::remove_all(procRoot / "104");
    makeProcEntry(procRoot, "105", "C:\\x\\RobloxStudioInstaller.exe", wanted);
    assert(prefixHasSessionHolderIn(procRoot.string(), wanted));

    // A missing /proc root is "nothing running", not an error.
    assert(!prefixHasSessionHolderIn((tmp / "nope").string(), wanted));

    // collectPrefixPidsIn(): everything in the prefix, not just the four
    // session-holder images -- Terminate has to take wineserver and the Wine
    // services down too, or the prefix is left half-alive.
    {
        const fs::path killRoot = tmp / "proc_kill";
        fs::create_directories(killRoot);
        makeProcEntry(killRoot, "10", "C:\\x\\RobloxPlayerBeta.exe", wanted);
        makeProcEntry(killRoot, "11", "C:\\windows\\system32\\services.exe", wanted);
        makeProcEntry(killRoot, "12", "/usr/bin/wineserver", wanted);
        makeProcEntry(killRoot, "13", "C:\\x\\RobloxPlayerBeta.exe", other);
        makeProcEntry(killRoot, "14", "/usr/bin/firefox", "");

        std::vector<int> pids = collectPrefixPidsIn(killRoot.string(), wanted);
        std::sort(pids.begin(), pids.end());
        assert((pids == std::vector<int>{10, 11, 12}));

        // A prefix nothing is running in yields nothing rather than everything.
        assert(collectPrefixPidsIn(killRoot.string(), (tmp / "nope").string()).empty());
        // An empty prefix must never match every process on the machine.
        assert(collectPrefixPidsIn(killRoot.string(), "").empty());
    }

    // prefixSessions(): counts and pids per app, which the Home cards and the
    // Stop buttons both read.
    {
        const fs::path censusRoot = tmp / "proc_census";
        fs::create_directories(censusRoot);
        makeProcEntry(censusRoot, "20", "C:\\x\\RobloxStudioBeta.exe", wanted);
        makeProcEntry(censusRoot, "21", "C:\\x\\RobloxStudioBeta.exe", wanted);
        makeProcEntry(censusRoot, "22", "C:\\x\\RobloxPlayerBeta.exe", wanted);
        makeProcEntry(censusRoot, "23", "C:\\x\\RobloxPlayerInstaller.exe", wanted);
        makeProcEntry(censusRoot, "24", "C:\\x\\RobloxStudioBeta.exe", other);
        makeProcEntry(censusRoot, "25", "C:\\windows\\system32\\explorer.exe", wanted);

        PrefixSessions sessions = prefixSessionsIn(censusRoot.string(), wanted);
        assert(sessions.studio == 2);
        assert(sessions.player == 1);
        assert(sessions.installers == 1);
        std::vector<int> studioPids = sessions.studioPids;
        std::sort(studioPids.begin(), studioPids.end());
        assert((studioPids == std::vector<int>{20, 21}));
        assert((sessions.playerPids == std::vector<int>{22}));

        // The trailing-slash spelling the layer sets WINEPREFIX with must match.
        assert(prefixSessionsIn(censusRoot.string(), wanted + "/").studio == 2);

        // An installer alone is not a session, but it is still a holder.
        fs::remove_all(censusRoot / "20");
        fs::remove_all(censusRoot / "21");
        fs::remove_all(censusRoot / "22");
        PrefixSessions installerOnly = prefixSessionsIn(censusRoot.string(), wanted);
        assert(installerOnly.player == 0 && installerOnly.studio == 0);
        assert(installerOnly.installers == 1);
        assert(prefixHasSessionHolderIn(censusRoot.string(), wanted));

        // A missing /proc root is "nothing running", not an error.
        PrefixSessions none = prefixSessionsIn((tmp / "nope").string(), wanted);
        assert(none.player == 0 && none.studio == 0 && none.installers == 0);

        // An empty prefix must never match every process on the machine.
        PrefixSessions empty = prefixSessionsIn(censusRoot.string(), "");
        assert(empty.player == 0 && empty.studio == 0 && empty.installers == 0);
    }

    // An environ that cannot be read is "not ours", never a match -- it is how
    // another user's processes and kernel threads look.
    {
        const fs::path blindRoot = tmp / "proc_blind";
        fs::create_directories(blindRoot / "30");
        writeNulJoined(blindRoot / "30" / "cmdline", {"C:\\x\\RobloxPlayerBeta.exe"});
        // No environ file at all.
        assert(prefixSessionsIn(blindRoot.string(), wanted).player == 0);
        assert(!prefixHasSessionHolderIn(blindRoot.string(), wanted));
    }

    // envValueFromEnviron(): the general form wineprefixFromEnviron now uses.
    {
        // Built up rather than written as one literal: a NUL inside a string
        // literal needs an exact byte count, which is a silent bug waiting to
        // happen every time the test is edited.
        std::string blob;
        for (const char* entry : {"PATH=/usr/bin", "TUXBLOX_PREFIX=/opt/tb/runtime", "HOME=/root"}) {
            blob += entry;
            blob.push_back('\0');
        }
        assert(envValueFromEnviron(blob, "TUXBLOX_PREFIX") == "/opt/tb/runtime");
        assert(envValueFromEnviron(blob, "PATH") == "/usr/bin");
        assert(envValueFromEnviron(blob, "MISSING").empty());
        // A key that is only a prefix of a real one must not match it.
        assert(envValueFromEnviron(blob, "TUXBLOX").empty());
    }

    // launchJoinsPrefix(): which launches join a drive somebody else set up.
    {
        PrefixSessions empty;
        assert(!launchJoinsPrefix(LaunchTarget::Player, empty));
        assert(!launchJoinsPrefix(LaunchTarget::Studio, empty));

        // A Player launch ignores the Player it is about to replace, so it still
        // owns the drive and still tears it down on the way out.
        PrefixSessions onePlayer;
        onePlayer.player = 1;
        onePlayer.playerPids = {10};
        assert(!launchJoinsPrefix(LaunchTarget::Player, onePlayer));
        // Studio makes no such claim -- several Studio sessions run at once.
        assert(launchJoinsPrefix(LaunchTarget::Studio, onePlayer));

        // Anything else holding the drive makes both launches guests.
        PrefixSessions withStudio;
        withStudio.studio = 1;
        withStudio.studioPids = {11};
        assert(launchJoinsPrefix(LaunchTarget::Player, withStudio));
        assert(launchJoinsPrefix(LaunchTarget::Studio, withStudio));

        PrefixSessions withInstaller;
        withInstaller.installers = 1;
        assert(launchJoinsPrefix(LaunchTarget::Player, withInstaller));
        assert(launchJoinsPrefix(LaunchTarget::Studio, withInstaller));

        PrefixSessions twoStudio;
        twoStudio.studio = 2;
        twoStudio.studioPids = {12, 13};
        assert(launchJoinsPrefix(LaunchTarget::Studio, twoStudio));
    }

    // layerProcessesForIn(): which compatibility-layer process is driving which
    // session. Identified by the resolved /proc/<pid>/exe rather than by argv[0],
    // so a different spelling of the same path still matches.
    {
        const fs::path layerRoot = tmp / "proc_layer";
        const fs::path installDir = tmp / "install";
        const fs::path layerBinary = installDir / "compat" / "main";
        fs::create_directories(layerBinary.parent_path());
        std::ofstream(layerBinary) << "not really a binary";

        const std::string prefixEnv = (installDir / "runtime").string();

        auto makeLayerEntry = [&](const std::string& pid, const fs::path& exe,
                                  const std::string& targetExe, const std::string& tuxbloxPrefix) {
            fs::create_directories(layerRoot / pid);
            writeNulJoined(layerRoot / pid / "cmdline", {exe.string(), "run", "--immediate", targetExe});
            std::vector<std::string> env = {"PATH=/usr/bin"};
            if (!tuxbloxPrefix.empty()) env.push_back("TUXBLOX_PREFIX=" + tuxbloxPrefix);
            writeNulJoined(layerRoot / pid / "environ", env);
            fs::create_symlink(exe, layerRoot / pid / "exe");
        };

        makeLayerEntry("40", layerBinary, "/v/RobloxStudioBeta.exe", prefixEnv);
        makeLayerEntry("41", layerBinary, "/v/RobloxPlayerBeta.exe", prefixEnv);
        makeLayerEntry("42", layerBinary, "/v/RobloxStudioBeta.exe", prefixEnv);

        // Another install's layer process, same image, must be left alone --
        // stopping Studio here must not stop Studio there.
        const fs::path otherInstall = tmp / "install2";
        const fs::path otherBinary = otherInstall / "compat" / "main";
        fs::create_directories(otherBinary.parent_path());
        std::ofstream(otherBinary) << "another install";
        makeLayerEntry("43", otherBinary, "/v/RobloxStudioBeta.exe", (otherInstall / "runtime").string());

        // The same binary, pointed at a different drive by TUXBLOX_PREFIX.
        makeLayerEntry("44", layerBinary, "/v/RobloxStudioBeta.exe", (tmp / "elsewhere").string());

        // A Roblox process itself is not a layer process.
        makeProcEntry(layerRoot, "45", "C:\\v\\RobloxStudioBeta.exe", wanted);

        std::vector<int> studio = layerProcessesForIn(layerRoot.string(), installDir.string(),
                                                     LaunchTarget::Studio);
        std::sort(studio.begin(), studio.end());
        assert((studio == std::vector<int>{40, 42}));

        std::vector<int> player = layerProcessesForIn(layerRoot.string(), installDir.string(),
                                                     LaunchTarget::Player);
        assert((player == std::vector<int>{41}));

        // Nothing running for an install with no processes at all.
        assert(layerProcessesForIn(layerRoot.string(), (tmp / "install3").string(),
                                   LaunchTarget::Studio).empty());
        // A missing /proc root is "nothing running", not an error.
        assert(layerProcessesForIn((tmp / "nope").string(), installDir.string(),
                                   LaunchTarget::Studio).empty());
    }

    // Stopping an app with nothing running is a no-op that reports zero, not an
    // error -- the census a click acts on is up to a second old, so it can
    // always race a session that just exited.
    {
        const fs::path quietInstall = tmp / "install_quiet";
        fs::create_directories(quietInstall / "runtime" / "pfx");
        assert(stopPrefixSessions(quietInstall.string(), LaunchTarget::Player) == 0);
        assert(stopPrefixSessions(quietInstall.string(), LaunchTarget::Studio) == 0);
    }

    fs::remove_all(tmp);
    std::printf("prefix_session: all tests passed\n");
    return 0;
}
