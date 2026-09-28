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

#pragma once
#include <cstdint>
#include <ctime>
#include <string>
#include <vector>

namespace tuxblox {

// Every place Discord may put its socket on Linux, in the order they are tried: the plain one, Flatpak's, then Snap's.
std::vector<std::string> discordSocketCandidates(const std::string& runtimeDir);

// Discord's IPC frame: a little-endian opcode and length, then the JSON body.
std::string encodeFrame(uint32_t opcode, const std::string& payload);

// Talks to Discord over its local socket. Every operation is non-blocking: presence must never be able to delay a Roblox session.
class DiscordRpc {
public:
    explicit DiscordRpc(std::string applicationId);
    ~DiscordRpc();

    DiscordRpc(const DiscordRpc&) = delete;
    DiscordRpc& operator=(const DiscordRpc&) = delete;

    bool connected() const;

    // Connects if needed, waits for Discord to answer the handshake, and puts the last activity back after a reconnect. Doing nothing is the normal outcome.
    void poll(std::time_t now);

    // Sends an already-serialised activity object. False means it did not reach Discord, so the caller must not record it as published.
    bool send(const std::string& activityJson, std::time_t now);

    void clear(std::time_t now);

private:
    bool writeFrame(uint32_t opcode, const std::string& payload);
    // Discord discards anything sent before it has answered the handshake, so nothing goes out until its reply has been read.
    bool readyForActivity(std::time_t now);
    void connectIfNeeded(std::time_t now);
    bool sendActivity(const std::string& activityJson, std::time_t now);
    void republishIfNeeded(std::time_t now);
    void disconnect();

    std::string applicationId_;
    int fd_ = -1;
    std::time_t lastAttempt_ = 0;
    std::time_t connectedAt_ = 0;
    bool ready_ = false;
    std::string pendingReply_;
    // Kept so a connection that replaced a dropped one does not come back empty.
    std::string lastActivity_;
    bool publishedOnConnection_ = false;
};

} // namespace tuxblox
