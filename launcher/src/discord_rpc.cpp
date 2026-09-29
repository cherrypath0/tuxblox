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

#include <cstdlib>
#include <cstring>
#include <utility>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace tuxblox {

namespace {

const char *const SocketDirs[] = {
    "",
    "app/com.discordapp.Discord/",
    "snap.discord/",
};

// Discord's opcodes. Only these two are needed to publish presence.
const uint32_t OpHandshake = 0;
const uint32_t OpFrame = 1;

// Discord being absent is the normal case, so retries are occasional rather than eager.
const int ReconnectSeconds = 30;

// A connection Discord accepts but never answers would otherwise be held for the whole session, so one that does not become usable is dropped and retried.
const int ReadyTimeoutSeconds = 10;

std::string runtimeDir() {
    const char *pDir = std::getenv("XDG_RUNTIME_DIR");
    return pDir != nullptr ? std::string(pDir) : std::string();
}

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

DiscordRpc::DiscordRpc(std::string applicationId) : applicationId_(std::move(applicationId)) {}

DiscordRpc::~DiscordRpc() {
    disconnect();
}

bool DiscordRpc::connected() const {
    return fd_ >= 0;
}

void DiscordRpc::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    ready_ = false;
    pendingReply_.clear();
    publishedOnConnection_ = false;
}

bool DiscordRpc::readyForActivity(std::time_t now) {
    if (ready_) {
        return true;
    }
    if (fd_ < 0) {
        return false;
    }

    char buffer[1024];
    for (;;) {
        const ssize_t got = ::recv(fd_, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (got <= 0) {
            break;
        }
        pendingReply_.append(buffer, static_cast<size_t>(got));
        if (got < static_cast<ssize_t>(sizeof(buffer))) {
            break;
        }
    }

    if (pendingReply_.size() >= 8) {
        uint32_t length = 0;
        std::memcpy(&length, pendingReply_.data() + 4, 4);
        if (pendingReply_.size() >= 8 + static_cast<size_t>(length)) {
            ready_ = true;
            pendingReply_.clear();
        }
    }
    if (!ready_ && now - connectedAt_ >= ReadyTimeoutSeconds) {
        disconnect();
    }
    return ready_;
}

void DiscordRpc::connectIfNeeded(std::time_t now) {
    // No application id means presence was never set up, so there is nothing to connect to Discord about.
    if (applicationId_.empty()) {
        return;
    }
    if (fd_ >= 0 || now - lastAttempt_ < ReconnectSeconds) {
        return;
    }
    lastAttempt_ = now;

    for (const std::string& path : discordSocketCandidates(runtimeDir())) {
        if (path.size() >= sizeof(sockaddr_un{}.sun_path)) {
            continue;
        }
        const int fd = ::socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            continue;
        }
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        if (::connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0) {
            ::close(fd);
            continue;
        }
        fd_ = fd;
        connectedAt_ = now;
        if (!writeFrame(OpHandshake, "{\"v\":1,\"client_id\":\"" + applicationId_ + "\"}")) {
            disconnect();
            continue;
        }
        return;
    }
}

void DiscordRpc::republishIfNeeded(std::time_t now) {
    // A connection that replaced a dropped one starts with no activity on it, and the state may never change again to prompt one.
    if (fd_ < 0 || publishedOnConnection_ || lastActivity_.empty()) {
        return;
    }
    sendActivity(lastActivity_, now);
}

void DiscordRpc::poll(std::time_t now) {
    connectIfNeeded(now);
    republishIfNeeded(now);
}

bool DiscordRpc::writeFrame(uint32_t opcode, const std::string& payload) {
    if (fd_ < 0) {
        return false;
    }
    const std::string frame = encodeFrame(opcode, payload);
    size_t sent = 0;
    while (sent < frame.size()) {
        // MSG_NOSIGNAL, not write(): Discord closing its end must fail this call, never raise SIGPIPE and kill the launcher.
        const ssize_t wrote = ::send(fd_, frame.data() + sent, frame.size() - sent, MSG_NOSIGNAL);
        if (wrote > 0) {
            sent += static_cast<size_t>(wrote);
            continue;
        }
        // A blocked write is dropped rather than waited on, because a stalled Discord must never stall Roblox.
        return false;
    }
    return true;
}

bool DiscordRpc::send(const std::string& activityJson, std::time_t now) {
    lastActivity_ = activityJson;
    connectIfNeeded(now);
    return sendActivity(activityJson, now);
}

bool DiscordRpc::sendActivity(const std::string& activityJson, std::time_t now) {
    if (fd_ < 0 || !readyForActivity(now)) {
        return false;
    }
    const std::string payload = "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"" + std::to_string(now) +
                                "\",\"args\":{\"pid\":" + std::to_string(::getpid()) +
                                ",\"activity\":" + activityJson + "}}";
    if (!writeFrame(OpFrame, payload)) {
        disconnect();
        return false;
    }
    publishedOnConnection_ = true;
    return true;
}

void DiscordRpc::clear(std::time_t now) {
    if (fd_ < 0) {
        return;
    }
    const std::string payload = "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"" + std::to_string(now) +
                                "\",\"args\":{\"pid\":" + std::to_string(::getpid()) + "}}";
    // An activity with no object is how Discord is told to drop the presence.
    writeFrame(OpFrame, payload);
    disconnect();
}

} // namespace tuxblox
