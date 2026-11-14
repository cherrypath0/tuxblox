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
#include "session_log_match.h"
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

    // selectSessionLogFiles(): out of a folder holding every kind of file, a Player session takes
    // its own log and nothing else -- not the certificate bundle Roblox keeps there, not an
    // installer log, not Studio's, not an earlier run's, and not a later instance's.
    {
        const std::string ours = "0.747_20261113T154434Z_Player_C452A_last.log";
        std::vector<std::string> names = {
            "cacert.pem",
            "RobloxStudioInstaller_DA84A.log",
            "0.747_20261113T150505Z_Studio_F2B8B_last.log",
            "0.747_20261113T143447Z_Player_2CEF3_last.log",
            ours,
            "0.747_20261113T160000Z_Player_9F0A1_last.log",
        };
        auto result = selectSessionLogFiles(names, robloxLogStartTime(ours) - 1, true);
        assert(result.size() == 1);
        assert(result[0] == ours);
    }

    // selectSessionLogFiles(): the crash reporter Roblox starts alongside the client belongs to the
    // same session, so its log is carried too.
    {
        const std::string ours = "0.747_20261113T154434Z_Player_C452A_last.log";
        const std::string crash = "0.747_20261113T154434Z_Player_A330E_CrashHandler_last.log";
        auto result = selectSessionLogFiles({crash, ours}, robloxLogStartTime(ours) - 1, true);
        assert(result.size() == 2);
    }

    // selectSessionLogFiles(): a Studio session never takes a Player log running beside it.
    {
        const std::string studio = "0.747_20261113T150505Z_Studio_F2B8B_last.log";
        const std::string player = "0.747_20261113T150506Z_Player_C452A_last.log";
        auto result = selectSessionLogFiles({player, studio}, robloxLogStartTime(studio) - 1, false);
        assert(result.size() == 1);
        assert(result[0] == studio);
    }

    // selectSessionLogFiles(): nothing to choose from, and nothing old enough, both give nothing.
    {
        assert(selectSessionLogFiles({}, 100, true).empty());
        assert(selectSessionLogFiles({"0.747_20261113T143447Z_Player_2CEF3_last.log"},
                                     2000000000, true)
                   .empty());
    }

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

        const std::string oldLog = "0.747_20261113T143447Z_Player_2CEF3_last.log";
        const std::string ours = "0.747_20261113T154434Z_Player_C452A_last.log";
        const std::time_t sessionStart = robloxLogStartTime(ours) - 1;

        {
            std::ofstream out(robloxLogs / oldLog);
            out << "stale content from a previous session";
        }
        setMtime(robloxLogs / oldLog, sessionStart - 3600);

        {
            std::ofstream out(robloxLogs / ours);
            out << "roblox's own log for this session";
        }
        // An earlier session's log is touched again while this one runs, which is exactly what made
        // the modification time useless for telling the two apart.
        setMtime(robloxLogs / oldLog, sessionStart + 5);
        setMtime(robloxLogs / ours, sessionStart + 5);

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "tuxblox/proton wrapper output\n";
        }

        auto appended =
            appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string(), true);
        assert(appended.size() == 1);
        assert(appended[0] == ours);

        std::string contents = readFile(destLog);
        assert(contents.find("stale content from a previous session") == std::string::npos);
        auto wrapperPos = contents.find("tuxblox/proton wrapper output");
        auto separatorPos = contents.find("=== ROBLOX LOG: " + ours + " ===");
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

        auto appended = appendRobloxSessionLogs(installDir.string(), 1000, destLog.string(), true);
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

        const std::string big = "0.747_20261113T154434Z_Player_C452A_last.log";
        const std::time_t sessionStart = robloxLogStartTime(big) - 1;

        std::string bigContent;
        bigContent.reserve(150000);
        for (int i = 0; i < 150000; ++i) bigContent += static_cast<char>('A' + (i % 26));

        {
            std::ofstream out(robloxLogs / big, std::ios::binary);
            out << bigContent;
        }
        setMtime(robloxLogs / big, sessionStart + 5);

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "wrapper\n";
        }

        auto appended =
            appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string(), true);
        assert(appended.size() == 1);

        std::string contents = readFile(destLog);
        assert(contents.find(bigContent) != std::string::npos);

        fs::remove_all(installDir);
    }

    // Roblox keeps its certificate bundle in the same folder as its logs, and rewrites it during a
    // session. It is not a log and must never be pasted into one.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_capture_stray";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const std::string ours = "0.747_20261113T154434Z_Player_C452A_last.log";
        const std::time_t sessionStart = robloxLogStartTime(ours) - 1;

        {
            std::ofstream out(robloxLogs / "cacert.pem");
            out << "-----BEGIN CERTIFICATE-----\n";
        }
        {
            std::ofstream out(robloxLogs / "RobloxStudioInstaller_DA84A.log");
            out << "installer chatter\n";
        }
        {
            std::ofstream out(robloxLogs / ours);
            out << "[FLog::Output] this session\n";
        }
        for (const char* name : {"cacert.pem", "RobloxStudioInstaller_DA84A.log", ours.c_str()}) {
            setMtime(robloxLogs / name, sessionStart + 5);
        }

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "wrapper\n";
        }

        auto appended =
            appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string(), true);
        assert(appended.size() == 1);
        assert(appended[0] == ours);

        std::string contents = readFile(destLog);
        assert(contents.find("BEGIN CERTIFICATE") == std::string::npos);
        assert(contents.find("installer chatter") == std::string::npos);
        assert(contents.find("[FLog::Output] this session") != std::string::npos);

        fs::remove_all(installDir);
    }

    // A second session writing in the same folder keeps its own log: a Player session running
    // beside Studio must not swallow Studio's, which is how one log ended up inside three.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_capture_beside";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const std::string ours = "0.747_20261113T154434Z_Player_C452A_last.log";
        const std::string theirs = "0.747_20261113T154500Z_Studio_F2B8B_last.log";
        const std::time_t sessionStart = robloxLogStartTime(ours) - 1;

        {
            std::ofstream out(robloxLogs / ours);
            out << "[FLog::Output] the player\n";
        }
        {
            std::ofstream out(robloxLogs / theirs);
            out << "[FLog::Output] studio next door\n";
        }
        setMtime(robloxLogs / ours, sessionStart + 5);
        setMtime(robloxLogs / theirs, sessionStart + 9);

        fs::path destLog = installDir / "wrapper.log";
        {
            std::ofstream out(destLog);
            out << "wrapper\n";
        }

        auto appended =
            appendRobloxSessionLogs(installDir.string(), sessionStart, destLog.string(), true);
        assert(appended.size() == 1);
        assert(appended[0] == ours);
        assert(readFile(destLog).find("studio next door") == std::string::npos);

        fs::remove_all(installDir);
    }

    // One log per session: what has been appended is not left behind where it came from.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_removal";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const std::string mine = "0.740_20261006T185941Z_Player_9C018_last.log";
        {
            std::ofstream out(robloxLogs / mine);
            out << "[FLog::Output] already in our log\n";
        }

        // Exactly one log for this target, so it is unambiguously this session's.
        assert(removeAppendedRobloxLogs(installDir.string(), {mine}, true) == 1);
        assert(!fs::exists(robloxLogs / mine));

        // The client's log and the crash reporter's are both this session's, so both go once they
        // have been copied -- what was selected has already been checked for whose it is.
        const std::string a = "0.740_20261006T190000Z_Player_AAAAA_last.log";
        const std::string b = "0.740_20261006T190000Z_Player_BBBBB_CrashHandler_last.log";
        for (const std::string& name : {a, b}) {
            std::ofstream out(robloxLogs / name);
            out << "x\n";
        }
        assert(removeAppendedRobloxLogs(installDir.string(), {a, b}, true) == 2);
        assert(!fs::exists(robloxLogs / a) && !fs::exists(robloxLogs / b));

        // One name belonging to another app poisons nothing around it: it stays, the rest still go.
        const std::string mixedMine = "0.740_20261006T192000Z_Player_DDDDD_last.log";
        const std::string mixedOther = "0.740_20261006T192000Z_Studio_EEEEE_last.log";
        for (const std::string& name : {mixedMine, mixedOther}) {
            std::ofstream out(robloxLogs / name);
            out << "x\n";
        }
        assert(removeAppendedRobloxLogs(installDir.string(), {mixedMine, mixedOther}, true) == 1);
        assert(!fs::exists(robloxLogs / mixedMine));
        assert(fs::exists(robloxLogs / mixedOther));

        // Studio's log is never this Player session's to remove, and the other way round.
        const std::string studio = "0.740_20261006T191000Z_Studio_CCCCC_last.log";
        {
            std::ofstream out(robloxLogs / studio);
            out << "x\n";
        }
        assert(removeAppendedRobloxLogs(installDir.string(), {studio}, true) == 0);
        assert(fs::exists(robloxLogs / studio));
        assert(removeAppendedRobloxLogs(installDir.string(), {studio}, false) == 1);
        assert(!fs::exists(robloxLogs / studio));

        // A name carrying no target at all is not a session log and is left where it is.
        const std::string odd = "something-else.log";
        {
            std::ofstream out(robloxLogs / odd);
            out << "x\n";
        }
        assert(removeAppendedRobloxLogs(installDir.string(), {odd}, true) == 0);
        assert(fs::exists(robloxLogs / odd));

        // Roblox keeps its crash reports beside its logs, and a directory is never a log to remove.
        fs::create_directories(robloxLogs / "crashes");
        {
            std::ofstream out(robloxLogs / "crashes" / "settings.dat");
            out << "x";
        }
        assert(removeAppendedRobloxLogs(installDir.string(), {"crashes"}, true) == 0);
        assert(fs::exists(robloxLogs / "crashes" / "settings.dat"));

        assert(removeAppendedRobloxLogs(installDir.string(), {}, true) == 0);
        fs::remove_all(installDir);
    }

    // An append that could not be written must not report success, or the only copy of the log is
    // deleted on the strength of it. /dev/full accepts the open and fails every write.
    {
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_roblox_log_fulldisk";
        fs::remove_all(installDir);
        fs::path robloxLogs = fs::path(robloxLogsDir(installDir.string()));
        fs::create_directories(robloxLogs);

        const fs::path source = robloxLogs / "0.740_20261006T185941Z_Player_9C018_last.log";
        {
            std::ofstream out(source);
            out << std::string(200000, 'x') << "\n";
        }
        setMtime(source, std::time(nullptr));

        if (fs::exists("/dev/full")) {
            auto appended = appendRobloxSessionLogs(installDir.string(), 0, "/dev/full", true);
            assert(appended.empty());
            assert(fs::exists(source));
        }
        fs::remove_all(installDir);
    }

    printf("roblox_log_capture: all tests passed\n");
    return 0;
}
