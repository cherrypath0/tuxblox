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
void setLaunchEnv(const std::string& installDir);

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
