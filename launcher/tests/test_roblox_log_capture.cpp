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
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <utime.h>

namespace fs = std::filesystem;

namespace {

void setMtime(const fs::path& path, std::time_t t) {
    struct utimbuf times{t, t};
    utime(path.c_str(), &times);
}

std::string readFile(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

} // namespace

int main() {
    using namespace tuxblox;

    // selectSessionLogFiles(): keeps only entries at/after sessionStart, preserving order.
    {
        std::vector<std::pair<std::string, std::time_t>> entries = {
            {"a.log", 100}, {"b.log", 200}, {"c.log", 300},
        };
        auto result = selectSessionLogFiles(entries, 200);
        assert(result.size() == 2);
        assert(result[0] == "b.log");
        assert(result[1] == "c.log");
    }

    // selectSessionLogFiles(): no entries -> empty.
    { assert(selectSessionLogFiles({}, 100).empty()); }

    // selectSessionLogFiles(): everything older than sessionStart -> empty.
    {
        std::vector<std::pair<std::string, std::time_t>> entries = {{"old.log", 50}};
        assert(selectSessionLogFiles(entries, 100).empty());
    }

    // robloxLogsDir(): matches plan/todo.md item 10's path exactly.
    {
        assert(robloxLogsDir("/home/x/.tuxblox") ==
               "/home/x/.tuxblox/runtime/pfx/drive_c/users/user/AppData/Local/Roblox/logs");
    }

    // appendRobloxSessionLogs(): only the current-session log gets merged in,
    // after the existing wrapper content, with a separator header; a
    // leftover log from an earlier session is left out entirely.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_capture";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const std::time_t sessionStart = 2000000000;

        {
            std::ofstream out(robloxLogs / "old-session.log");
            out << "stale content from a previous session";
        }
        setMtime(robloxLogs / "old-session.log", sessionStart - 3600);

        {
            std::ofstream out(robloxLogs / "current-session.log");
            out << "roblox's own log for this session";
        }
        setMtime(robloxLogs / "current-session.log", sessionStart + 5);

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "tuxblox/proton wrapper output\n";
        }

        auto appended = appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string());
        assert(appended.size() == 1);
        assert(appended[0] == "current-session.log");

        std::string contents = readFile(destLog);
        assert(contents.find("stale content from a previous session") == std::string::npos);
        auto wrapperPos = contents.find("tuxblox/proton wrapper output");
        auto separatorPos = contents.find("=== ROBLOX LOG: current-session.log ===");
        auto robloxPos = contents.find("roblox's own log for this session");
        assert(wrapperPos != std::string::npos);
        assert(separatorPos != std::string::npos);
        assert(robloxPos != std::string::npos);
        assert(wrapperPos < separatorPos && separatorPos < robloxPos); // wrapper content stays first

        fs::remove_all(installDir);
    }

    // appendRobloxSessionLogs(): no Roblox logs directory at all -> no-op,
    // destination untouched, no throw.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_capture_missing";
        fs::remove_all(installDir);
        fs::create_directories(installDir);
        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "original\n";
        }

        auto appended = appendRobloxSessionLogs(installDir.string(), 1000, destLog.string());
        assert(appended.empty());
        assert(readFile(destLog) == "original\n");

        fs::remove_all(installDir);
    }

    // appendRobloxSessionLogs(): a source file that straddles the internal
    // copy-chunk boundary must still transfer byte-for-byte, unbroken --
    // proves the chunked streaming copy doesn't drop/duplicate/reorder
    // bytes at a chunk edge (the classic bug class fixed-buffer I/O
    // introduces). 150000 bytes is deliberately not a multiple of any
    // "round" buffer size.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_capture_large";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const std::time_t sessionStart = 2000000000;

        std::string bigContent;
        bigContent.reserve(150000);
        for (int i = 0; i < 150000; ++i) bigContent += static_cast<char>('A' + (i % 26));

        {
            std::ofstream out(robloxLogs / "big-session.log", std::ios::binary);
            out << bigContent;
        }
        setMtime(robloxLogs / "big-session.log", sessionStart + 5);

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "wrapper\n";
        }

        auto appended = appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string());
        assert(appended.size() == 1);

        std::string contents = readFile(destLog);
        assert(contents.find(bigContent) != std::string::npos);

        fs::remove_all(installDir);
    }

    printf("roblox_log_capture: all tests passed\n");
    return 0;
}
