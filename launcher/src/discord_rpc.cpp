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

#include <cstring>

namespace tuxblox {

namespace {

const char *const SocketDirs[] = {
    "",
    "app/com.discordapp.Discord/",
    "snap.discord/",
};

} // namespace

std::vector<std::string> discordSocketCandidates(const std::string& runtimeDir) {
    std::vector<std::string> paths;
    if (runtimeDir.empty()) {
        return paths;
    }

    std::string base = runtimeDir;
    if (base.back() != '/') {
        base += '/';
    }

    for (const char *pDir : SocketDirs) {
        for (int index = 0; index < 10; index++) {
            paths.push_back(base + pDir + "discord-ipc-" + std::to_string(index));
        }
    }
    return paths;
}

std::string encodeFrame(uint32_t opcode, const std::string& payload) {
    const uint32_t length = static_cast<uint32_t>(payload.size());
    std::string frame(8, '\0');
    std::memcpy(&frame[0], &opcode, 4);
    std::memcpy(&frame[4], &length, 4);
    frame += payload;
    return frame;
}

} // namespace tuxblox
