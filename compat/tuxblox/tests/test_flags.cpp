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
#include "flags.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    const fs::path dir = fs::temp_directory_path() / "tuxblox_test_layer_flags";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // Started by hand, without the launcher: neither variable is set, so nothing is
    // asked. This is every `main run something.exe` out of build/.
    {
        unsetenv("TUXBLOX_FLAGS_CHANNEL");
        unsetenv("TUXBLOX_FLAGS_RELEASED");
        bool asked = false;
        resetFlags();
        initLayerFlags(dir.string(), [&](const std::string&, int) { asked = true; return std::string(); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(!asked);
    }

    // A channel but no release: still nothing asked. An install that does not say what
    // it is must not reach the network from the launch path.
    {
        setenv("TUXBLOX_FLAGS_CHANNEL", "stable", 1);
        unsetenv("TUXBLOX_FLAGS_RELEASED");
        bool asked = false;
        resetFlags();
        initLayerFlags(dir.string(), [&](const std::string&, int) { asked = true; return std::string(); });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(!asked);
    }

    // Both set: the launcher is telling us this is a real install on a known channel.
    {
        setenv("TUXBLOX_FLAGS_CHANNEL", "canary", 1);
        setenv("TUXBLOX_FLAGS_RELEASED", "1", 1);
        std::string seen;
        int budget = 0;
        resetFlags();
        initLayerFlags(dir.string(), [&](const std::string& url, int seconds) {
            seen = url;
            budget = seconds;
            return std::string("{}");
        });
        (void)flagBool("tbxFlagDisableUpdater");
        assert(seen == clientFlagsUrl("canary", flagArch()));
        // Two seconds, because this is the path between pressing Play and Roblox starting.
        assert(budget == 2);
    }

    // The compiled-in defaults answer whatever happens.
    {
        assert(!flagString("tbxStringPrivacyPolicyVersion").empty());
    }

    fs::remove_all(dir);
    printf("layer_flags: all tests passed\n");
    return 0;
}
