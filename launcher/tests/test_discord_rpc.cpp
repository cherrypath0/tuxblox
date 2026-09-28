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

#include "discord_rpc.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

int main() {
    // The three layouts Discord uses on Linux, in the order they are tried.
    const auto paths = tuxblox::discordSocketCandidates("/run/user/1000");
    assert(paths.size() == 30);
    assert(paths[0] == "/run/user/1000/discord-ipc-0");
    assert(paths[9] == "/run/user/1000/discord-ipc-9");
    assert(paths[10] == "/run/user/1000/app/com.discordapp.Discord/discord-ipc-0");
    assert(paths[20] == "/run/user/1000/snap.discord/discord-ipc-0");

    // A trailing slash on the runtime directory must not double up.
    const auto slashed = tuxblox::discordSocketCandidates("/run/user/1000/");
    assert(slashed[0] == "/run/user/1000/discord-ipc-0");

    // An empty runtime directory yields nothing to try rather than paths rooted at "/".
    assert(tuxblox::discordSocketCandidates("").empty());

    const std::string frame = tuxblox::encodeFrame(1, "{\"a\":1}");
    assert(frame.size() == 8 + 7);
    uint32_t opcode = 0;
    uint32_t length = 0;
    std::memcpy(&opcode, frame.data(), 4);
    std::memcpy(&length, frame.data() + 4, 4);
    assert(opcode == 1);
    assert(length == 7);
    assert(frame.substr(8) == "{\"a\":1}");

    const std::string empty = tuxblox::encodeFrame(2, "");
    assert(empty.size() == 8);

    return 0;
}
