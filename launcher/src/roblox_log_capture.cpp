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

#include "roblox_log_capture.h"
#include "prefix_user.h"
#include "session_log_match.h"
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

namespace tuxblox {

const char* const kCreatedLogFileLine = "TuxBlox: created logfile: ";

namespace {

// Roblox starts its crash reporter alongside the client, so one beginning long after belongs to another instance.
constexpr std::time_t kCrashReporterLeadSeconds = 60;

constexpr size_t kMaxAnnouncedLogs = 4;

// Announced as Roblox starts, so near the front; the rest is not re-read every pass.
constexpr std::streamoff kAnnouncementScanBytes = 4 * 1024 * 1024;

constexpr size_t kMaxAnnouncementBytes = 4096;

} // namespace

std::vector<std::string> announcedLogFileNames(const std::string& capturedLogPath) {
    std::vector<std::string> names;
    if (capturedLogPath.empty()) return names;

    std::ifstream in(capturedLogPath, std::ios::binary);
    if (!in) return names;

    const std::string marker = kCreatedLogFileLine;
    auto take = [&](const std::string& line) {
        if (names.size() >= kMaxAnnouncedLogs) return;
        const size_t at = line.find(marker);
        if (at == std::string::npos) return;

        std::string path = line.substr(at + marker.size());
        while (!path.empty() && (path.back() == '\r' || path.back() == ' ')) path.pop_back();
        const size_t slash = path.find_last_of("\\/");
        const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        if (name.empty()) return;
        if (std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    };

    // Not getline: a source with no line break is read into memory whole.
    char chunk[8192];
    std::string pending;
    std::streamoff scanned = 0;

    while (names.size() < kMaxAnnouncedLogs && scanned < kAnnouncementScanBytes) {
        in.read(chunk, sizeof(chunk));
        const std::streamsize got = in.gcount();
        if (got <= 0) break;
        scanned += got;

        for (std::streamsize i = 0; i < got; ++i) {
            if (chunk[i] == '\n') {
                take(pending);
                pending.clear();
            } else if (pending.size() < kMaxAnnouncementBytes) {
                pending.push_back(chunk[i]);
            }
        }
    }
    take(pending);
    return names;
}

std::vector<std::string> selectSessionLogFiles(const std::vector<std::string>& names,
                                               std::time_t sessionStart, bool player) {
    const char* marker = player ? "_Player_" : "_Studio_";

    std::string session, crash;
    std::time_t sessionAt = 0, crashAt = 0;

    for (const std::string& name : names) {
        if (name.find(marker) == std::string::npos) continue;
        const std::time_t startedAt = robloxLogStartTime(name);
        if (startedAt == 0 || startedAt < sessionStart) continue;

        // The earliest log at or after this launch is its own; anything later was written by an instance that started after it.
        if (isCrashHandlerLog(name)) {
            if (crash.empty() || startedAt < crashAt) { crash = name; crashAt = startedAt; }
        } else if (session.empty() || startedAt < sessionAt) {
            session = name;
            sessionAt = startedAt;
        }
    }

    std::vector<std::string> result;
    if (session.empty()) return result;
    result.push_back(session);
    if (!crash.empty() && crashAt - sessionAt <= kCrashReporterLeadSeconds) result.push_back(crash);
    std::sort(result.begin(), result.end());
    return result;
}

std::string robloxLogsDir(const std::string& installDir) {
    return robloxDataDir(installDir) + "/logs";
}

namespace {

constexpr size_t kCopyChunkBytes = 64 * 1024;

// Streams src onto the end of dest in fixed-size chunks -- never holds more
// than one chunk of src in memory, regardless of its total size.
bool streamAppendFile(std::ofstream& dest, const std::string& srcPath) {
    std::ifstream src(srcPath, std::ios::binary);
    if (!src) return false;

    std::vector<char> buf(kCopyChunkBytes);
    while (src) {
        src.read(buf.data(), static_cast<std::streamsize>(buf.size()));
        std::streamsize got = src.gcount();
        if (got > 0) dest.write(buf.data(), got);
        // A full disk fails every write silently, and reporting that as appended is what would let the only copy of the log be deleted.
        if (!dest) return false;
    }
    dest.flush();
    return static_cast<bool>(dest);
}

} // namespace

std::vector<std::string> appendRobloxSessionLogs(const std::string& installDir, std::time_t sessionStart,
                                                  const std::string& destLogPath, bool player) {
    std::vector<std::string> appended;

    std::string dir = robloxLogsDir(installDir);
    std::error_code ec;
    if (!fs::exists(dir, ec) || ec) return appended;

    std::vector<std::string> entries;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        entries.push_back(entry.path().filename().string());
    }
    std::sort(entries.begin(), entries.end()); // deterministic order regardless of directory iteration order

    // The capture is this session's own, so what it names beats the guess below.
    std::vector<std::string> qualifying;
    for (const std::string& name : announcedLogFileNames(destLogPath)) {
        if (std::find(entries.begin(), entries.end(), name) != entries.end()) qualifying.push_back(name);
    }
    if (qualifying.empty()) qualifying = selectSessionLogFiles(entries, sessionStart, player);
    if (qualifying.empty()) return appended;

    std::ofstream dest(destLogPath, std::ios::binary | std::ios::app);
    if (!dest) return appended;

    for (const auto& name : qualifying) {
        if (streamAppendFile(dest, (fs::path(dir) / name).string())) {
            appended.push_back(name);
        }
    }
    return appended;
}

int removeAppendedRobloxLogs(const std::string& installDir, const std::vector<std::string>& appended,
                             bool player) {
    const char* marker = player ? "_Player_" : "_Studio_";
    const fs::path dir = robloxLogsDir(installDir);
    int removed = 0;

    for (const std::string& name : appended) {
        if (name.find(marker) == std::string::npos) continue;
        const fs::path path = dir / name;
        std::error_code ec;
        if (!fs::is_regular_file(path, ec)) continue;
        if (fs::remove(path, ec)) ++removed;
    }
    return removed;
}

} // namespace tuxblox
