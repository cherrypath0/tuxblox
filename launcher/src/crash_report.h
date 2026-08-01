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
