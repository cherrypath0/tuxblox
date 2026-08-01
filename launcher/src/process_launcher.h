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
#include <vector>
#include <mutex>
#include <sys/types.h>
#include "lnk_resolver.h"

namespace tuxblox {

// A process-group-exit event, reported once per running->stopped
// transition. `stopRequested` distinguishes a user/app-initiated stop()
// (SIGTERM, possibly escalated to SIGKILL) from the process dying on its
// own -- callers use this to avoid treating an ordinary Stop click as a
// crash just because the exit code that results from it happens to be
// non-zero (e.g. 143 "Terminated").
struct ExitEvent {
    int exitCode = 0;
    bool stopRequested = false;
};

// Maps a process exit code to the short human-readable title used in the
// exit-code-monitoring popup (e.g. 139 -> "Segmentation Fault"). See
// plan/plan.txt item 1 for the table this implements. Returns "Unknown
// exit code" for anything not in that table.
const char* exitCodeTitle(int exitCode);

class TrackedProcess {
public:
    // `env` entries are "KEY=VALUE" strings, applied via setenv() in the
    // child after fork(), before exec -- never in the parent. See
    // Finding 6, 2026-07-28 final review: setenv()/getenv() are not
    // thread-safe in glibc, and the parent process has other threads
    // (update-check thread inside curl, render thread inside SDL) that may
    // call getenv() concurrently.
    //
    // `logFilePath`, if non-empty, is where the child's stdout/stderr are
    // redirected (dup2'd in the child before exec) -- used to capture a
    // crash log callers can point the user at.
    bool start(const std::vector<std::string>& argv, const std::vector<std::string>& env = {},
               const std::string& logFilePath = "");
    void poll();
    bool isRunning() const;
    void stop();

    // Returns and clears the exit event from the most recent running->
    // stopped transition seen by poll(), if any -- so each transition is
    // reported to a caller exactly once.
    std::optional<ExitEvent> takeExitEvent();

private:
    mutable std::mutex mutex_;
    pid_t pid_ = -1;
    bool running_ = false;
    bool stopRequested_ = false;
    std::optional<ExitEvent> pendingExitEvent_;
};

struct LaunchOutcome {
    bool ok = false;
    std::string errorMessage;
    std::string logPath; // where this launch's stdout/stderr were captured, valid only if ok
};

std::string resolveOrBootstrapExePath(LaunchTarget target, const std::string& installDir);
std::string protonBinaryPath(const std::string& installDir);

// Env vars ("KEY=VALUE" strings) Proton itself needs to locate the prefix
// and render correctly. Callers must apply these only to the environment of
// the "proton run" child (setenv() in a soon-to-exec()'d child, or an envp
// passed to exec*e()) -- never to the launcher's own process, since that
// process may go on to spawn other, non-Proton children (curl update
// checks, etc.) that have no business seeing Wine/Proton/Steam-Play-shaped
// env vars. See item 33 in plan/plan.txt.
std::vector<std::string> launchEnvVars(const std::string& installDir, LaunchTarget target);

class ProcessLauncher {
public:
    explicit ProcessLauncher(std::string installDir);

    bool pollIsRunning(LaunchTarget target);
    // `extraEnv` ("KEY=VALUE" strings, already parsed by the caller -- see
    // settings::parseEnvPairs) are the user-supplied "Proton Environment
    // Variables" from Settings, appended on top of launchEnvVars(). They
    // reach only this "proton run" child, same as the rest of the vector --
    // never the launcher's own process.
    LaunchOutcome launch(LaunchTarget target, const std::string& uri = "",
                          const std::vector<std::string>& extraEnv = {});
    void stop(LaunchTarget target);

    // See TrackedProcess::takeExitEvent().
    std::optional<ExitEvent> takeExitEvent(LaunchTarget target);

private:
    TrackedProcess& processFor(LaunchTarget target);

    std::string installDir_;
    TrackedProcess player_;
    TrackedProcess studio_;
};

} // namespace tuxblox
