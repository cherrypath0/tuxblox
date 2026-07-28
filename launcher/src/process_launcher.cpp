#include "process_launcher.h"
#include "downloader.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

bool TrackedProcess::start(const std::vector<std::string>& argv, const std::vector<std::string>& env) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (running_) return false;
    }
    if (argv.empty()) return false;

    pid_t pid = fork();
    if (pid < 0) return false;

    if (pid == 0) {
        // Own process group so stop() can SIGTERM exactly this subtree
        // without touching a sibling target or the launcher itself.
        setpgid(0, 0);
        // Applied here, in the single-threaded child, rather than in the
        // (multi-threaded) parent -- see the comment on the header
        // declaration and Finding 6, 2026-07-28 final review.
        for (const auto& kv : env) {
            auto pos = kv.find('=');
            if (pos != std::string::npos) {
                setenv(kv.substr(0, pos).c_str(), kv.substr(pos + 1).c_str(), 1);
            }
        }
        std::vector<char*> cargv;
        cargv.reserve(argv.size() + 1);
        for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
        cargv.push_back(nullptr);
        execvp(cargv[0], cargv.data());
        _exit(127); // only reached if execvp failed
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        pid_ = pid;
        running_ = true;
    }
    return true;
}

void TrackedProcess::poll() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;
    int status = 0;
    pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == pid_ || r < 0) {
        running_ = false;
        pid_ = -1;
    }
}

bool TrackedProcess::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

void TrackedProcess::stop() {
    pid_t pidToKill;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        pidToKill = pid_;
    }
    kill(-pidToKill, SIGTERM);
    // Wine/Windows apps under Proton often ignore SIGTERM -- give the
    // process group ~2s to exit gracefully, then force-kill anything
    // still alive in it.
    for (int i = 0; i < 20; ++i) {
        poll();
        if (!isRunning()) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (isRunning()) {
        kill(-pidToKill, SIGKILL);
    }
}

std::string protonBinaryPath(const std::string& installDir) {
    return installDir + "/ProtonBuild/dist/proton";
}

void setLaunchEnv(const std::string& installDir) {
    setenv("STEAM_COMPAT_DATA_PATH", (installDir + "/runtime").c_str(), 1);
    setenv("STEAM_COMPAT_CLIENT_INSTALL_PATH", installDir.c_str(), 1);
    setenv("PROTON_LOG_DIR", (installDir + "/logs").c_str(), 1);
    setenv("DXVK_ASYNC", "1", 1);
}

std::string resolveOrBootstrapExePath(LaunchTarget target, const std::string& installDir) {
    const std::string driveC = installDir + "/runtime/pfx/drive_c";
    std::string found = resolveExePath(target, driveC);
    if (!found.empty()) return found;

    const bool isPlayer = target == LaunchTarget::Player;
    const std::string cacheDir = installDir + (isPlayer ? "/RobloxPlayer" : "/RobloxStudio");
    const std::string installerName = isPlayer ? "RobloxPlayerInstaller.exe" : "RobloxStudioInstaller.exe";
    const std::string installerPath = cacheDir + "/" + installerName;
    const std::string url = std::string("https://setup.rbxcdn.com/") + installerName;

    if (fs::exists(installerPath)) return installerPath;

    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    std::atomic<bool> noCancel{false};
    auto outcome = downloadFile(url, installerPath, [](uint64_t, uint64_t) {}, &noCancel);
    if (outcome.result != DownloadResult::Ok) return "";
    return installerPath;
}

ProcessLauncher::ProcessLauncher(std::string installDir) : installDir_(std::move(installDir)) {}

TrackedProcess& ProcessLauncher::processFor(LaunchTarget target) {
    return target == LaunchTarget::Player ? player_ : studio_;
}

bool ProcessLauncher::pollIsRunning(LaunchTarget target) {
    TrackedProcess& p = processFor(target);
    p.poll();
    return p.isRunning();
}

LaunchOutcome ProcessLauncher::launch(LaunchTarget target, const std::string& uri) {
    TrackedProcess& p = processFor(target);
    if (p.isRunning()) {
        return {false, "already running"};
    }

    std::string exePath = resolveOrBootstrapExePath(target, installDir_);
    if (exePath.empty()) {
        return {false, "could not resolve or download the Roblox executable"};
    }

    // Same 4 vars setLaunchEnv() sets, but applied in the child (via
    // TrackedProcess::start's env overlay) rather than here in the parent --
    // see Finding 6, 2026-07-28 final review. setLaunchEnv() itself is left
    // unchanged and is still used as-is by headless_launch.cpp, which is
    // single-threaded at that point and unaffected by this hazard.
    std::vector<std::string> env = {
        "STEAM_COMPAT_DATA_PATH=" + installDir_ + "/runtime",
        "STEAM_COMPAT_CLIENT_INSTALL_PATH=" + installDir_,
        "PROTON_LOG_DIR=" + installDir_ + "/logs",
        "DXVK_ASYNC=1",
    };

    std::vector<std::string> argv = {protonBinaryPath(installDir_), "run", exePath};
    if (!uri.empty()) argv.push_back(uri);

    if (!p.start(argv, env)) {
        return {false, "failed to start Proton process"};
    }
    return {true, ""};
}

void ProcessLauncher::stop(LaunchTarget target) {
    processFor(target).stop();
}

} // namespace tuxblox
