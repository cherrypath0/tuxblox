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

#include "support/sessions.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// Writes a NUL-delimited blob, the shape /proc/<pid>/cmdline and environ use.
static void writeNulJoined(const fs::path& path, const std::vector<std::string>& parts) {
    std::ofstream out(path, std::ios::binary);
    for (const std::string& part : parts) {
        out.write(part.data(), static_cast<std::streamsize>(part.size()));
        out.put('\0');
    }
}

static void makeProcEntry(const fs::path& procRoot, const std::string& pid,
                          const std::string& cmdline, const std::string& winePrefix) {
    fs::create_directories(procRoot / pid);
    writeNulJoined(procRoot / pid / "cmdline", {cmdline});
    std::vector<std::string> env = {"PATH=/usr/bin"};
    if (!winePrefix.empty()) env.push_back("WINEPREFIX=" + winePrefix);
    writeNulJoined(procRoot / pid / "environ", env);
}

int main() {
    using namespace tuxblox;

    assert(imageNameOf("C:\\users\\cherry\\RobloxPlayerBeta.exe") == "RobloxPlayerBeta.exe");
    assert(imageNameOf("/home/cherry/RobloxStudioBeta.exe") == "RobloxStudioBeta.exe");
    assert(imageNameOf("main") == "main");

    assert(imageIsClient("robloxplayerbeta.exe"));
    assert(imageIsClient("RobloxStudioBeta.exe"));
    assert(!imageIsClient("robloxcrashhandler.exe"));
    assert(imageIsInstaller("RobloxPlayerInstaller.exe"));
    assert(!imageIsInstaller("robloxplayerbeta.exe"));

    const fs::path tmp = fs::temp_directory_path() / "tuxblox_sessions_test";
    fs::remove_all(tmp);
    const fs::path procRoot = tmp / "proc";
    fs::create_directories(procRoot);
    const std::string wanted = (tmp / "runtime" / "pfx").string();
    const std::string other = (tmp / "elsewhere" / "pfx").string();

    assert(prefixSessionHoldersIn(procRoot, wanted).empty());

    makeProcEntry(procRoot, "50", "C:\\v\\RobloxPlayerBeta.exe", wanted);
    makeProcEntry(procRoot, "51", "C:\\v\\RobloxStudioBeta.exe", wanted);
    makeProcEntry(procRoot, "52", "C:\\v\\RobloxStudioInstaller.exe", wanted);
    makeProcEntry(procRoot, "53", "C:\\v\\RobloxPlayerBeta.exe", other);
    makeProcEntry(procRoot, "54", "C:\\v\\RobloxCrashHandler.exe", wanted);
    fs::create_directories(procRoot / "self");

    std::vector<SessionHolder> holders = prefixSessionHoldersIn(procRoot, wanted);
    assert(holders.size() == 3);
    int clients = 0;
    for (const SessionHolder& holder : holders)
        if (holder.client) clients++;
    assert(clients == 2);

    // The layer sets WINEPREFIX with a trailing slash and normalises it away.
    assert(prefixSessionHoldersIn(procRoot, wanted + "/").size() == 3);

    // sessionsToReplace(): only a Player launch replaces anything, and only the
    // live Player clients -- never Studio, never an installer mid-handover.
    std::vector<std::string> replace = sessionsToReplace("RobloxPlayerBeta.exe", holders);
    assert((replace == std::vector<std::string>{"50"}));
    assert(sessionsToReplace("RobloxStudioBeta.exe", holders).empty());
    assert(sessionsToReplace("RobloxPlayerInstaller.exe", holders).empty());
    assert(sessionsToReplace("RobloxPlayerBeta.exe", {}).empty());

    // driveExitOnClose(): who takes the virtual drive down as a session closes.
    // The last one out takes it down; a guest never does; and a session that owns
    // the drive but is not the last out has to stay until the others leave --
    // exiting then would leave the drive running with nobody left to close it.
    std::vector<SessionHolder> studioLeft = {SessionHolder{"51", "robloxstudiobeta.exe", true}};
    std::vector<SessionHolder> installerLeft = {SessionHolder{"52", "robloxstudioinstaller.exe", false}};
    assert(driveExitOnClose(true, {}) == DriveExit::TearDownNow);
    assert(driveExitOnClose(false, {}) == DriveExit::Leave);
    assert(driveExitOnClose(false, studioLeft) == DriveExit::Leave);
    assert(driveExitOnClose(true, studioLeft) == DriveExit::WaitThenTearDown);
    assert(driveExitOnClose(true, installerLeft) == DriveExit::WaitThenTearDown);

    // otherLayerProcesses(): which layer process is running which session.
    {
        const fs::path layerRoot = tmp / "proc_layer";
        const fs::path layerBinary = tmp / "install" / "compat" / "main";
        fs::create_directories(layerBinary.parent_path());
        std::ofstream(layerBinary) << "not really a binary";
        const std::string prefixEnv = (tmp / "install" / "runtime").string();

        auto makeLayerEntry = [&](const std::string& pid, const fs::path& exe,
                                  const std::string& targetExe, const std::string& tuxbloxPrefix) {
            fs::create_directories(layerRoot / pid);
            writeNulJoined(layerRoot / pid / "cmdline", {exe.string(), "run", targetExe});
            writeNulJoined(layerRoot / pid / "environ",
                           {"PATH=/usr/bin", "TUXBLOX_PREFIX=" + tuxbloxPrefix});
            fs::create_symlink(exe, layerRoot / pid / "exe");
        };

        makeLayerEntry("60", layerBinary, "/v/RobloxPlayerBeta.exe", prefixEnv);
        makeLayerEntry("61", layerBinary, "/v/RobloxStudioBeta.exe", prefixEnv);
        // Another install's layer process, same image -- must be left alone.
        const fs::path otherBinary = tmp / "install2" / "compat" / "main";
        fs::create_directories(otherBinary.parent_path());
        std::ofstream(otherBinary) << "another install";
        makeLayerEntry("62", otherBinary, "/v/RobloxPlayerBeta.exe", (tmp / "install2" / "runtime").string());
        // The same binary pointed at another drive.
        makeLayerEntry("63", layerBinary, "/v/RobloxPlayerBeta.exe", (tmp / "nowhere").string());
        // This process itself is never in the answer.
        makeLayerEntry("64", layerBinary, "/v/RobloxPlayerBeta.exe", prefixEnv);

        std::vector<std::string> found =
            otherLayerProcesses(layerRoot, layerBinary, prefixEnv, "RobloxPlayerBeta.exe", 64);
        assert((found == std::vector<std::string>{"60"}));

        std::vector<std::string> studio =
            otherLayerProcesses(layerRoot, layerBinary, prefixEnv, "RobloxStudioBeta.exe", 64);
        assert((studio == std::vector<std::string>{"61"}));

        assert(otherLayerProcesses(tmp / "nope", layerBinary, prefixEnv,
                                   "RobloxPlayerBeta.exe", 64).empty());
    }

    fs::remove_all(tmp);
    std::printf("sessions: all tests passed\n");
    return 0;
}
