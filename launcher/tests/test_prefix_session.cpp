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

    // proton.py sets WINEPREFIX with a trailing slash and normpaths it away on
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

    fs::remove_all(tmp);
    std::printf("prefix_session: all tests passed\n");
    return 0;
}
