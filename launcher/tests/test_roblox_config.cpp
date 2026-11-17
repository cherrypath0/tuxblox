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

#include "roblox_config.h"
#include "roblox_deploy.h"
#include <cassert>
#include <cstdio>
#include <string>

int main() {
    using namespace tuxblox;

    // Where the recorded version history is read from. The suffix guard is the same one
    // TUXBLOX_RELEASES_CDN_HOST uses: anything that can set the variable usually cannot
    // also own a tuxblox.net name, which is what the allow-list is there to require.
    {
        assert(configBaseUrlFor(nullptr) == "https://config.tuxblox.net");
        assert(configBaseUrlFor("") == "https://config.tuxblox.net");
        assert(configBaseUrlFor("config.devcdn1.tuxblox.net") == "https://config.devcdn1.tuxblox.net");
        assert(configBaseUrlFor("CONFIG.TUXBLOX.NET") == "https://CONFIG.TUXBLOX.NET");

        // Ignored, never honoured: each of these falls back to the real host.
        for (const char* bad : {"evil.com", "config.tuxblox.net.evil.com", "eviltuxblox.net",
                                 "https://config.tuxblox.net", "config.tuxblox.net:8080",
                                 "config.tuxblox.net/path", "user@config.tuxblox.net"}) {
            assert(configBaseUrlFor(bad) == "https://config.tuxblox.net");
        }
    }

    // configPreviousUrl(): the channel is normalised, so "production" reaches live's history.
    {
        assert(configPreviousUrl("live", "WindowsPlayer") ==
               "https://config.tuxblox.net/v1/roblox/client-version/live/WindowsPlayer/previous");
        assert(configPreviousUrl("production", "WindowsStudio64") ==
               "https://config.tuxblox.net/v1/roblox/client-version/live/WindowsStudio64/previous");
        assert(configPreviousUrl("", "WindowsPlayer") ==
               "https://config.tuxblox.net/v1/roblox/client-version/live/WindowsPlayer/previous");
        assert(configPreviousUrl("zbeta", "WindowsPlayer") ==
               "https://config.tuxblox.net/v1/roblox/client-version/zbeta/WindowsPlayer/previous");
    }

    // parsePreviousHash(): the endpoint answers in Roblox's own shape, so the launcher
    // reads it with the parser it already has. A 404 body is a reason, not a build.
    {
        const std::string ok = R"({"version":"0.746.0.7461267",)"
                               R"("clientVersionUpload":"version-f0ea5da9412f4e35",)"
                               R"("bootstrapperVersion":""})";
        assert(parsePreviousHash(ok).value() == "version-f0ea5da9412f4e35");
        assert(!parsePreviousHash(R"({"error":"no earlier build has been recorded"})").has_value());
        assert(!parsePreviousHash("not json at all").has_value());
        assert(!parsePreviousHash("").has_value());
        assert(!parsePreviousHash("{}").has_value());
    }

    printf("roblox_config: all tests passed\n");
    return 0;
}
