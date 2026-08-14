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
#include <optional>
#include <string>
#include "json.hpp"
#include "lnk_resolver.h"
#include "system_info.h"

namespace tuxblox {

struct CrashReport {
    std::string launcherVersion;
    std::string protonVersion; // empty if unknown
    LaunchTarget target;
    int protonExitCode = 0;             // Proton's own 0/success, 1/proton-error, 2/process-error contract
    std::optional<int> robloxExitCode;  // relayed "TUXBLOX_REAL_EXIT_CODE=" value; see findRealExitCodeInLog, nullopt if never found
    std::string logPath;                // local path; a tail of this file's contents is uploaded as its own multipart file part
    SystemInfo systemInfo;
};

// Exposed for testing. Builds the JSON metadata part of the upload -- every
// field except the log itself, which travels as a separate multipart file
// part (see uploadCrashReport). `timestampIso` is injected rather than
// computed internally so this stays a pure, deterministic function.
nlohmann::json buildReportJson(const CrashReport& report, const std::string& timestampIso);

// Exposed for testing. Reads at most the last `maxBytes` of `path`.
// Best-effort: a missing or unreadable log file just means an empty tail,
// not a failed upload.
std::string readLogTail(const std::string& path, long maxBytes);

// Best-effort, fire-and-forget multipart/form-data POST of `report` to
// telemetry.tuxblox.net: one "data" part carrying buildReportJson()'s JSON,
// one "logfile" part carrying the tail of `report.logPath`'s actual file
// content (the full log, sent as a file -- not embedded/truncated into the
// JSON). Never throws and gives up silently on any failure (network error,
// host unreachable, non-2xx response) -- a failed telemetry upload must
// never surface to the user or block the caller for long. Meant to be
// called on its own thread; touches no shared state.
void uploadCrashReport(const CrashReport& report);

} // namespace tuxblox
