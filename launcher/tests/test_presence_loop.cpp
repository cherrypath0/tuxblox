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

#include "presence_loop.h"

#include <cassert>
#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

int main() {
    // Reading the log is what drives both features, so either one being on has to keep a session watched.
    const tuxblox::Settings off;
    assert(!tuxblox::presenceWatchWanted(off));

    tuxblox::Settings rpcOnly;
    rpcOnly.discordRpc = true;
    assert(tuxblox::presenceWatchWanted(rpcOnly));

    tuxblox::Settings regionOnly;
    regionOnly.serverRegionNotify = true;
    assert(tuxblox::presenceWatchWanted(regionOnly));

    tuxblox::Settings both;
    both.discordRpc = true;
    both.serverRegionNotify = true;
    assert(tuxblox::presenceWatchWanted(both));

    // The sequence a real session writes, captured 2026-10-06: the play session starts, then the server is named.
    const std::filesystem::path log =
        std::filesystem::temp_directory_path() / "tuxblox_test_presence_loop.log";
    {
        std::ofstream out(log);
        out << "2026-10-06T20:31:49.892Z,11.892516,00d4,6 [FLog::SessionTransitionFSM] Entered play session.\n"
            << "2026-10-06T20:31:50.122Z,12.122521,00d4,7 [FLog::Network] serverId: 128.116.31.33|61772\n";
    }

    std::string askedFor, notified;
    tuxblox::ServerRegionWatcher watcher(
        true,
        [&](const std::string& url) {
            askedFor = url;
            return std::string("{\"ASN\":\"AS22697\",\"owner\":\"Roblox\",\"location\":\"London, "
                               "England\",\"country\":\"United Kingdom\"}");
        },
        [&](const std::string& title) { notified = title; });

    // A session started from a web link has no launcher process tracking it, so the loop is told when it has ended rather than asking a ProcessLauncher.
    int ticks = 0;
    tuxblox::runPresenceLoop(".", tuxblox::LaunchTarget::Player, regionOnly, std::time(nullptr),
                             [&] { return ++ticks <= 2; }, std::move(watcher), log.string());

    assert(askedFor == "https://geo.tuxblox.net/128.116.31.33");
    assert(notified == "Connected to a server in London, England, United Kingdom");

    std::filesystem::remove(log);
    printf("presence_loop: ok\n");
    return 0;
}
