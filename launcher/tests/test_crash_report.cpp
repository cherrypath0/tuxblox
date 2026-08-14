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

#include "crash_report.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <optional>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // readLogTail(): content shorter than the cap comes back whole.
    {
        fs::path path = fs::temp_directory_path() / "tuxblox_test_crash_report_short_log.txt";
        {
            std::ofstream out(path);
            out << "short log content";
        }
        assert(readLogTail(path.string(), 1024) == "short log content");
        fs::remove(path);
    }

    // readLogTail(): content longer than the cap is truncated to just the tail.
    {
        fs::path path = fs::temp_directory_path() / "tuxblox_test_crash_report_long_log.txt";
        {
            std::ofstream out(path);
            out << "0123456789ABCDEF"; // 16 bytes
        }
        assert(readLogTail(path.string(), 6) == "ABCDEF");
        fs::remove(path);
    }

    // readLogTail(): missing file -> empty, not a crash/throw.
    { assert(readLogTail("/nonexistent/tuxblox_test_no_such_log.txt", 1024) == ""); }

    // buildReportJson(): every metadata field lands under its expected key,
    // and roblox_exit_code is present (not omitted) when known.
    {
        CrashReport report;
        report.launcherVersion = "0.2.0";
        report.protonVersion = "9.0-3";
        report.target = LaunchTarget::Player;
        report.protonExitCode = 2;
        report.robloxExitCode = -2147467260;
        report.logPath = "/tmp/whatever.log"; // unused by buildReportJson itself
        report.systemInfo.os = "Arch Linux";
        report.systemInfo.displayServer = "Wayland";
        report.systemInfo.desktopEnvironment = "KDE";
        report.systemInfo.gpu = "NVIDIA 610.43.03 proprietary";
        report.systemInfo.hasRootPrivileges = false;

        auto j = buildReportJson(report, "2026-08-15T12:34:56Z");
        assert(j.at("launcher_version").get<std::string>() == "0.2.0");
        assert(j.at("proton_version").get<std::string>() == "9.0-3");
        assert(j.at("target").get<std::string>() == "player");
        assert(j.at("proton_exit_code").get<int>() == 2);
        assert(j.at("roblox_exit_code").get<int>() == -2147467260);
        assert(j.at("timestamp").get<std::string>() == "2026-08-15T12:34:56Z");
        assert(j.at("os").get<std::string>() == "Arch Linux");
        assert(j.at("display_server").get<std::string>() == "Wayland");
        assert(j.at("desktop_environment").get<std::string>() == "KDE");
        assert(j.at("gpu").get<std::string>() == "NVIDIA 610.43.03 proprietary");
        assert(j.at("has_root").get<bool>() == false);

        // The full log now travels as its own multipart file part, not
        // embedded/truncated into this JSON -- must never reappear here.
        assert(!j.contains("log_tail"));
    }

    // buildReportJson(): Studio target maps correctly, and an unknown
    // Roblox exit code serializes as JSON null rather than being omitted
    // or silently defaulted to 0 (which would read as a real exit code).
    {
        CrashReport report;
        report.target = LaunchTarget::Studio;
        report.protonExitCode = 1;
        report.robloxExitCode = std::nullopt;

        auto j = buildReportJson(report, "2026-08-15T00:00:00Z");
        assert(j.at("target").get<std::string>() == "studio");
        assert(j.at("roblox_exit_code").is_null());
    }

    printf("crash_report: all tests passed\n");
    return 0;
}
