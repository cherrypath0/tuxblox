#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <sys/types.h>
#include "lnk_resolver.h"

namespace tuxblox {

class TrackedProcess {
public:
    // `env` entries are "KEY=VALUE" strings, applied via setenv() in the
    // child after fork(), before exec -- never in the parent. See
    // Finding 6, 2026-07-28 final review: setenv()/getenv() are not
    // thread-safe in glibc, and the parent process has other threads
    // (update-check thread inside curl, render thread inside SDL) that may
    // call getenv() concurrently.
    bool start(const std::vector<std::string>& argv, const std::vector<std::string>& env = {});
    void poll();
    bool isRunning() const;
    void stop();

private:
    mutable std::mutex mutex_;
    pid_t pid_ = -1;
    bool running_ = false;
};

struct LaunchOutcome {
    bool ok = false;
    std::string errorMessage;
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
    LaunchOutcome launch(LaunchTarget target, const std::string& uri = "");
    void stop(LaunchTarget target);

private:
    TrackedProcess& processFor(LaunchTarget target);

    std::string installDir_;
    TrackedProcess player_;
    TrackedProcess studio_;
};

} // namespace tuxblox
