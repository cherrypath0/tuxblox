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
// plan/plan.txt item 1 for the table this implements. Returns nullptr for
// anything not in that table -- callers show the bare code rather than
// inventing a description for a code we don't actually know the meaning of.
const char* exitCodeTitle(int exitCode);

// Proton's own process exit code is now a fixed 0/success, 1/proton-error,
// 2/process-error contract (see ProtonSource/proton's exit-code contract
// note on Session.run()) -- it no longer carries the wrapped process's real,
// non-truncated exit code (e.g. Hyperion's -2147467260). That value is
// relayed separately as a "TUXBLOX_REAL_EXIT_CODE=<n>" marker line on
// Proton's own stderr, which ends up in the crash log at `logPath` (same
// file TrackedProcess::start() captures). Scans the log for the last such
// marker; returns nullopt if none is present (e.g. the failure never
// reached ntdll at all, or Proton itself is what failed -- exitCode == 1).
std::optional<int> findRealExitCodeInLog(const std::string& logPath);

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
    bool wasBootstrapInstall = false;
};

// If versions.json has an active pin for `target` AND that version's exe
// still exists on disk, returns its full path. Otherwise returns an empty
// string -- callers fall back to resolveOrBootstrapExePath's existing
// lnk-resolution/official-installer-bootstrap path.
std::string resolveActiveVersionExePath(LaunchTarget target, const std::string& installDir);

std::string resolveOrBootstrapExePath(LaunchTarget target, const std::string& installDir);
std::string protonBinaryPath(const std::string& installDir);

// Path of the crash/stdout log for one launch. Carries the launcher's pid
// because several instances can run at once (a second Studio goes through
// Proton's "runinprefix" verb, see ProcessLauncher::launch) and two launches
// in the same second would otherwise dup2() into the same file.
std::string launchLogPath(const std::string& installDir, LaunchTarget target);

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
