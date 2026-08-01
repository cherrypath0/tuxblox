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
#include <string>
#include "lnk_resolver.h"

namespace tuxblox {

struct CrashReport {
    std::string launcherVersion;
    std::string protonVersion; // empty if unknown
    LaunchTarget target;
    int exitCode = 0;
    std::string logPath; // local path; only its tail is actually uploaded
};

// Best-effort, fire-and-forget POST of `report` to telemetry.tuxblox.net.
// Never throws and gives up silently on any failure (network error, host
// unreachable, non-2xx response) -- a failed telemetry upload must never
// surface to the user or block the caller for long. Meant to be called on
// its own thread; touches no shared state.
void uploadCrashReport(const CrashReport& report);

} // namespace tuxblox
