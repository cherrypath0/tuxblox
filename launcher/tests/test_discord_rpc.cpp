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
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

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

    // A client with nowhere to connect must stay quiet and cheap, which is the
    // normal case: most people do not have Discord running.
    {
        const std::string emptyDir = "/tmp/tuxblox-rpc-test-empty";
        std::filesystem::create_directories(emptyDir);
        setenv("XDG_RUNTIME_DIR", emptyDir.c_str(), 1);

        tuxblox::DiscordRpc rpc("1234567890");
        rpc.poll(1000);
        assert(!rpc.connected());
        // Sending while disconnected must be a no-op rather than an error.
        rpc.send("{\"state\":\"x\"}", 1000);
        assert(!rpc.connected());
        std::filesystem::remove_all(emptyDir);
    }

    // No application id means presence was never configured, so there must be
    // no connection attempt at all -- not a handshake Discord would reject.
    {
        const std::string dir = "/tmp/tuxblox-rpc-test-noid";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);

        const std::string sockPath = dir + "/discord-ipc-0";
        int server = socket(AF_UNIX, SOCK_STREAM, 0);
        assert(server >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);
        assert(bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        assert(listen(server, 1) == 0);

        tuxblox::DiscordRpc unconfigured("");
        unconfigured.poll(5000);
        assert(!unconfigured.connected());

        close(server);
        std::filesystem::remove_all(dir);
    }

    // With a socket present, the client connects and writes a handshake first.
    {
        const std::string dir = "/tmp/tuxblox-rpc-test-live";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        setenv("XDG_RUNTIME_DIR", dir.c_str(), 1);

        const std::string sockPath = dir + "/discord-ipc-0";
        int server = socket(AF_UNIX, SOCK_STREAM, 0);
        assert(server >= 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, sockPath.c_str(), sizeof(addr.sun_path) - 1);
        assert(bind(server, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        assert(listen(server, 1) == 0);

        tuxblox::DiscordRpc rpc("1234567890");
        rpc.poll(2000);
        assert(rpc.connected());

        int client = accept(server, nullptr, nullptr);
        assert(client >= 0);

        char buffer[512];
        ssize_t got = read(client, buffer, sizeof(buffer));
        assert(got > 8);
        uint32_t op = 0;
        std::memcpy(&op, buffer, 4);
        assert(op == 0); // handshake
        const std::string body(buffer + 8, static_cast<size_t>(got) - 8);
        assert(body.find("\"client_id\":\"1234567890\"") != std::string::npos);
        assert(body.find("\"v\":1") != std::string::npos);

        // Discord going away mid-session must not crash or stall; it must drop
        // the connection and be willing to try again later.
        close(client);
        close(server);
        std::filesystem::remove(sockPath);
        for (int i = 0; i < 20; i++) {
            rpc.send("{\"state\":\"x\"}", 3000 + i);
        }
        assert(!rpc.connected());

        std::filesystem::remove_all(dir);
    }

    return 0;
}
