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

#include "flag_startup.h"
#include "FlagManager.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // An installer that was never published asks nobody, whatever it is pointed at: it carries a
    // release baked in from the VERSION file, but only staging can write a channel. This test
    // binary is one of those, so initInstallerFlags() answers for itself here.
    {
        const fs::path dir = fs::temp_directory_path() / "tuxblox_test_installer_flags_fresh";
        fs::remove_all(dir);
        fs::create_directories(dir);
        bool asked = false;
        resetFlags();
        initInstallerFlags(dir.string(), "stable",
                           [&](const std::string&, int) { asked = true; return std::string(); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(!asked);
        fs::remove_all(dir);
    }

    // A published installer asks -- for the channel it was told to use, which is the one
    // --channel named, not the one it was itself published on.
    {
        const fs::path dir = fs::temp_directory_path() / "tuxblox_test_installer_flags_update";
        fs::remove_all(dir);
        fs::create_directories(dir);
        std::string seen;
        resetFlags();
        initInstallerFlagsFor(dir.string(), "canary", true,
                           [&](const std::string& url, int) { seen = url; return std::string("{}"); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(seen == clientFlagsUrl("canary", flagArch()));
        fs::remove_all(dir);
    }

    // dev is still accepted as an alias for experimental everywhere else, so it reaches
    // the server as typed and the server resolves it.
    {
        const fs::path dir = fs::temp_directory_path() / "tuxblox_test_installer_flags_dev";
        fs::remove_all(dir);
        fs::create_directories(dir);
        std::string seen;
        resetFlags();
        initInstallerFlagsFor(dir.string(), "dev", true,
                           [&](const std::string& url, int) { seen = url; return std::string("{}"); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(seen == clientFlagsUrl("dev", flagArch()));
        fs::remove_all(dir);
    }

    printf("installer_flags: all tests passed\n");
    return 0;
}
