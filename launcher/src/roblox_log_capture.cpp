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
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>

namespace fs = std::filesystem;

namespace tuxblox {

std::vector<std::string> selectSessionLogFiles(
    const std::vector<std::pair<std::string, std::time_t>>& entries, std::time_t sessionStart) {
    std::vector<std::string> result;
    for (const auto& [name, mtime] : entries) {
        if (mtime >= sessionStart) result.push_back(name);
    }
    return result;
}

std::string robloxLogsDir(const std::string& installDir) {
    return installDir + "/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/logs";
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
    }
    return true;
}

} // namespace

std::vector<std::string> appendRobloxSessionLogs(const std::string& installDir, std::time_t sessionStart,
                                                  const std::string& destLogPath) {
    std::vector<std::string> appended;

    std::string dir = robloxLogsDir(installDir);
    std::error_code ec;
    if (!fs::exists(dir, ec) || ec) return appended;

    std::vector<std::pair<std::string, std::time_t>> entries;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        struct stat st{};
        if (stat(entry.path().c_str(), &st) != 0) continue;
        entries.emplace_back(entry.path().filename().string(), st.st_mtime);
    }
    std::sort(entries.begin(), entries.end()); // deterministic order regardless of directory iteration order

    auto qualifying = selectSessionLogFiles(entries, sessionStart);
    if (qualifying.empty()) return appended;

    std::ofstream dest(destLogPath, std::ios::binary | std::ios::app);
    if (!dest) return appended;

    for (const auto& name : qualifying) {
        dest << "=== ROBLOX LOG: " << name << " ===\n";
        if (streamAppendFile(dest, (fs::path(dir) / name).string())) {
            appended.push_back(name);
        }
    }
    return appended;
}

} // namespace tuxblox
