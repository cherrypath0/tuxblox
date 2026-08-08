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

#include "process_launcher.h"
#include "downloader.h"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

const char* exitCodeTitle(int exitCode) {
    // See plan/plan.txt item 1 for this table.
    switch (exitCode) {
        case 0:   return "OK";
        case 1:   return "General Error";
        case 7:   return "Argument list too long";
        case 68:  return "Unknown host (DNS/resolution failure)";
        case 69:  return "Remote host unreachable";
        case 134: return "Aborted";
        case 137: return "Out of Memory";
        case 139: return "Segmentation Fault";
        case 143: return "Terminated";
        case 187: return "Session terminated by Hyperion";
        case 8:   return "Client crashed";
        case 200: return "Wineserver timeout";
        case 201: return "Integrity verification failure";
        case 202: return "Corrupt prefix";
        case 203: return "Session terminated for user safety";
        default:  return "Unknown exit code";
    }
}

std::optional<int> findRealExitCodeInLog(const std::string& logPath) {
    if (logPath.empty()) return std::nullopt;

    std::ifstream in(logPath);
    if (!in) return std::nullopt;

    constexpr const char* kMarker = "TUXBLOX_REAL_EXIT_CODE=";
    const size_t markerLen = std::strlen(kMarker);

    std::optional<int> found;
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, markerLen, kMarker) != 0) continue;
        try {
            found = std::stoi(line.substr(markerLen));
        } catch (const std::exception&) {
            // Malformed line -- ignore and keep whatever was found earlier.
        }
    }
    return found;
}

bool TrackedProcess::start(const std::vector<std::string>& argv, const std::vector<std::string>& env,
                            const std::string& logFilePath) {
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

        if (!logFilePath.empty()) {
            int fd = ::open(logFilePath.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
            if (fd >= 0) {
                dup2(fd, STDOUT_FILENO);
                dup2(fd, STDERR_FILENO);
                if (fd > STDERR_FILENO) close(fd);
            }
        }

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
        stopRequested_ = false;
        pendingExitEvent_.reset();
    }
    return true;
}

void TrackedProcess::poll() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!running_) return;
    int status = 0;
    pid_t r = waitpid(pid_, &status, WNOHANG);
    if (r == pid_) {
        int code = 0;
        if (WIFEXITED(status)) {
            code = WEXITSTATUS(status);
        } else if (WIFSIGNALED(status)) {
            code = 128 + WTERMSIG(status);
        }
        pendingExitEvent_ = ExitEvent{code, stopRequested_};
        running_ = false;
        pid_ = -1;
    } else if (r < 0) {
        // waitpid() itself failed (e.g. ECHILD) -- no meaningful exit code
        // to report, just stop tracking it.
        running_ = false;
        pid_ = -1;
    }
}

bool TrackedProcess::isRunning() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return running_;
}

std::optional<ExitEvent> TrackedProcess::takeExitEvent() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto ev = pendingExitEvent_;
    pendingExitEvent_.reset();
    return ev;
}

void TrackedProcess::stop() {
    pid_t pidToKill;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) return;
        stopRequested_ = true;
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

std::vector<std::string> launchEnvVars(const std::string& installDir, LaunchTarget target) {
    std::vector<std::string> env = {
        "STEAM_COMPAT_DATA_PATH=" + installDir + "/runtime",
        "STEAM_COMPAT_CLIENT_INSTALL_PATH=" + installDir,
        "PROTON_LOG_DIR=" + installDir + "/logs",
        "DXVK_ASYNC=1",
    };
    if (target == LaunchTarget::Studio) {
        // Studio's embedded WebView2 (login/create.roblox.com UI) appears to rely
        // on a DXGI composition swapchain; DXVK's CreateSwapChainForComposition
        // returns E_NOTIMPL unless this is enabled. Scoped to Studio only -- see
        // dxgi_factory.cpp's enableDummyCompositionSwapchain option.
        env.push_back("DXVK_CONFIG=dxgi.enableDummyCompositionSwapchain=True");
    }
    return env;
}

namespace {

// Copies a host-side exe (living outside the prefix entirely -- e.g. a
// freshly downloaded installer under installDir/RobloxPlayer/) into the
// prefix's own C: before it's ever handed to Proton. Launching an exe via
// its raw path outside C: relies on ntdll's \??\unix\ bridge, which stopped
// working for the executable IMAGE itself once the Z: drive was removed
// (plan/plan.txt item 20): the process silently fails to launch at all --
// no window, no trace, just an early exit (observed as exit code 245) --
// instead of erroring visibly. Staging a real copy inside C: sidesteps the
// bridge entirely. Mirrors launch.sh's own stageInPrefix(); keep both in
// sync if this changes.
std::string stageInPrefix(const std::string& installDir, const std::string& srcPath) {
    const std::string stageDir = installDir + "/runtime/pfx/drive_c/TuxBloxStaging";
    std::error_code ec;
    fs::create_directories(stageDir, ec);
    const std::string dest = stageDir + "/" + fs::path(srcPath).filename().string();
    fs::copy_file(srcPath, dest, fs::copy_options::overwrite_existing, ec);
    if (ec) return srcPath; // best-effort -- fall back to the raw path if the copy failed
    return dest;
}

} // namespace

std::string resolveOrBootstrapExePath(LaunchTarget target, const std::string& installDir) {
    const std::string driveC = installDir + "/runtime/pfx/drive_c";
    std::string found = resolveExePath(target, driveC);
    if (!found.empty()) return found; // already inside the prefix's C: -- no staging needed

    const bool isPlayer = target == LaunchTarget::Player;
    const std::string cacheDir = installDir + (isPlayer ? "/RobloxPlayer" : "/RobloxStudio");
    const std::string installerName = isPlayer ? "RobloxPlayerInstaller.exe" : "RobloxStudioInstaller.exe";
    const std::string installerPath = cacheDir + "/" + installerName;
    const std::string url = std::string("https://setup.rbxcdn.com/") + installerName;

    if (fs::exists(installerPath)) return stageInPrefix(installDir, installerPath);

    std::error_code ec;
    fs::create_directories(cacheDir, ec);
    std::atomic<bool> noCancel{false};
    auto outcome = downloadFile(url, installerPath, [](uint64_t, uint64_t) {}, &noCancel);
    if (outcome.result != DownloadResult::Ok) return "";
    return stageInPrefix(installDir, installerPath);
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

namespace {

std::string logTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    localtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", &tmBuf);
    return buf;
}

} // namespace

LaunchOutcome ProcessLauncher::launch(LaunchTarget target, const std::string& uri,
                                       const std::vector<std::string>& extraEnv) {
    TrackedProcess& p = processFor(target);
    if (p.isRunning()) {
        return {false, "already running", ""};
    }

    std::string exePath = resolveOrBootstrapExePath(target, installDir_);
    if (exePath.empty()) {
        return {false, "could not resolve or download the Roblox executable", ""};
    }

    // Applied in the child (via TrackedProcess::start's env overlay) rather
    // than here in the parent -- see Finding 6, 2026-07-28 final review:
    // setenv()/getenv() are not thread-safe in glibc, and this (parent)
    // process has other threads that may call getenv() concurrently.
    std::vector<std::string> env = launchEnvVars(installDir_, target);
    env.insert(env.end(), extraEnv.begin(), extraEnv.end());

    std::vector<std::string> argv = {protonBinaryPath(installDir_), "run", exePath};
    if (!uri.empty()) argv.push_back(uri);

    const std::string logsDir = installDir_ + "/logs";
    std::error_code ec;
    fs::create_directories(logsDir, ec);
    const std::string targetName = target == LaunchTarget::Player ? "Player" : "Studio";
    const std::string logPath = logsDir + "/Roblox" + targetName + "-CrashLog-" + logTimestamp() + ".log";

    if (!p.start(argv, env, logPath)) {
        return {false, "failed to start Proton process", ""};
    }
    return {true, "", logPath};
}

void ProcessLauncher::stop(LaunchTarget target) {
    processFor(target).stop();
}

std::optional<ExitEvent> ProcessLauncher::takeExitEvent(LaunchTarget target) {
    return processFor(target).takeExitEvent();
}

} // namespace tuxblox
