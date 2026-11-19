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

#include "flag_startup.h"
#include "process_launcher.h"
#include "i18n.h"
#include "root_guard.h"
#include "launch_paths.h"
#include "fastflag_file.h"
#include "mac_client.h"
#include "settings.h"

#include "downloader.h"
#include "install_paths.h"
#include "prefix_session.h"
#include "versions_manifest.h"
#include <atomic>
#include <chrono>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <filesystem>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

const char* exitCodeTitle(int exitCode) {
    switch (exitCode) {
        case 0:   return i18n::tr("OK");
        case 1:   return i18n::tr("General Error");
        case 127: return i18n::tr("Not Executable");
        case 132: return i18n::tr("Illegal CPU instruction");
        case 134: return i18n::tr("Aborted");
        case 137: return i18n::tr("Killed");
        case 139: return i18n::tr("Segmentation Fault");
        case 143: return i18n::tr("Terminated");
        // Windows statuses, which is what a crashing Roblox exits with and so what the popup actually shows: the codes above are the host's, and findRealExitCodeInLog never returns one of them.
        case static_cast<int>(0x80000003): return i18n::tr("Breakpoint");
        case static_cast<int>(0x80000101): return i18n::tr("Assertion failed");
        case static_cast<int>(0xC0000005): return i18n::tr("Access Violation");
        case static_cast<int>(0xC000001D): return i18n::tr("Illegal CPU instruction");
        case static_cast<int>(0xC0000094): return i18n::tr("Division by Zero");
        case static_cast<int>(0xC00000FD): return i18n::tr("Stack Overflow");
        case static_cast<int>(0xC0000135): return i18n::tr("Missing Library");
        case static_cast<int>(0xC0000139): return i18n::tr("Missing Library Entry Point");
        case static_cast<int>(0xC000013A): return i18n::tr("Interrupted");
        case static_cast<int>(0xC0000142): return i18n::tr("Library Failed to Start");
        case static_cast<int>(0xC0000374): return i18n::tr("Heap Corruption");
        case static_cast<int>(0xC0000409): return i18n::tr("Stack Buffer Overrun");
        default:  return nullptr; // not in the table -- caller shows the bare code, no made-up description
    }
}

void appendSessionLine(const std::string& logPath, const std::string& message) {
    if (logPath.empty()) return;

    // Counted from when this watcher started, which is the session it is writing about.
    static const std::chrono::steady_clock::time_point started = std::chrono::steady_clock::now();
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();

    struct timespec now {};
    struct tm utc {};
    if (::clock_gettime(CLOCK_REALTIME, &now) != 0 || ::gmtime_r(&now.tv_sec, &utc) == nullptr) return;

    char head[96];
    const int length = std::snprintf(
        head, sizeof(head), "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ,%.6f,%04x,6,Info TuxBlox: ",
        utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour, utc.tm_min, utc.tm_sec,
        static_cast<int>(now.tv_nsec / 1000000), elapsed,
        static_cast<unsigned>(::getpid()) & 0xffffu);
    if (length <= 0) return;

    std::ofstream out(logPath, std::ios::binary | std::ios::app);
    if (!out) return;
    out << std::string(head, static_cast<size_t>(length)) << message << "\n";
}

std::optional<int> findRealExitCodeInLog(const std::string& logPath) {
    if (logPath.empty()) return std::nullopt;

    std::ifstream in(logPath);
    if (!in) return std::nullopt;

    constexpr const char* kRealMarker = "TUXBLOX_REAL_EXIT_CODE=";
    constexpr const char* kWrapperMarker = "TUXBLOX_WRAPPER_EXIT_CODE=";
    const size_t realLen = std::strlen(kRealMarker);
    const size_t wrapperLen = std::strlen(kWrapperMarker);

    // Read both markers, take the last of each: Roblox's own Windows exit code
    // if it got far enough to report one, and otherwise the code Proton's
    // waitpid() saw. Proton emits the second on every non-zero exit, so this
    // only comes back empty when Proton is what failed.
    std::optional<int> real, wrapper;
    std::string line;
    while (std::getline(in, line)) {
        std::optional<int>* slot = nullptr;
        size_t markerLen = 0;
        if (line.compare(0, realLen, kRealMarker) == 0) {
            slot = &real;
            markerLen = realLen;
        } else if (line.compare(0, wrapperLen, kWrapperMarker) == 0) {
            slot = &wrapper;
            markerLen = wrapperLen;
        } else {
            continue;
        }
        try {
            *slot = std::stoi(line.substr(markerLen));
        } catch (const std::exception&) {
            // Malformed line -- ignore and keep whatever was found earlier.
        }
    }
    return real ? real : wrapper;
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

namespace {

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

// UTC, not local time: a log filename means the same thing to whoever reads
// it, wherever they are, which matters as soon as one is attached to a bug
// report. Roblox names its own session logs the same way.
std::string logTimestamp() {
    std::time_t t = std::time(nullptr);
    std::tm tmBuf{};
    gmtime_r(&t, &tmBuf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%dT%H%M%SZ", &tmBuf);
    return buf;
}

} // namespace

std::string launchLogPath(const std::string& installDir, LaunchTarget target) {
    const std::string targetName = target == LaunchTarget::Player ? "Player" : "Studio";
    return installDir + "/logs/" + targetName + "-" + logTimestamp() + "-" +
           std::to_string(getpid()) + ".log";
}

bool launchRegistryEditor(const std::string& installDir) {
    const std::string exePath = installDir + "/runtime/pfx/drive_c/windows/regedit.exe";
    std::error_code ec;
    if (!fs::exists(exePath, ec) || ec) return false;

    std::vector<std::string> argv = {compatBinaryPath(installDir), "run", "--immediate"};
    if (allowRoot()) argv.push_back("--allow-root");
    argv.push_back(exePath);

    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const std::string& argument : argv) cargv.push_back(const_cast<char*>(argument.c_str()));
    cargv.push_back(nullptr);
    // Either app gives the same two variables; only Studio's adds a setting for its web panels.
    const std::vector<std::string> env = launchEnvVars(installDir, LaunchTarget::Player);

    // Two forks and a session of its own, so closing the window leaves it running and no child is
    // left for this process to collect.
    const pid_t outer = fork();
    if (outer < 0) return false;
    if (outer == 0) {
        if (fork() == 0) {
            setsid();
            // In the child, single-threaded: setenv() is not safe beside the launcher's own threads.
            for (const std::string& pair : env) {
                const size_t equals = pair.find('=');
                if (equals != std::string::npos) {
                    setenv(pair.substr(0, equals).c_str(), pair.substr(equals + 1).c_str(), 1);
                }
            }
            execvp(cargv[0], cargv.data());
            _exit(127);
        }
        _exit(0);
    }
    int status = 0;
    while (waitpid(outer, &status, 0) < 0 && errno == EINTR) {
    }
    return true;
}

// The experimental macOS Player backend. The client is downloaded, verified
// and installed by macclient::ensureCurrent() before anything is started; a
// refusal there is a hard failure, never a reason to fall back to an
// unverified bundle. The Windows path below is untouched.
static LaunchOutcome launchMacPlayer(const std::string& installDir, TrackedProcess& p,
                                     const std::string& uri,
                                     const std::vector<std::string>& extraEnv) {
    const macclient::EnsureResult prepared = macclient::ensureCurrent(installDir, "");
    if (!prepared.ok)
        return {false, std::string(i18n::tr("could not prepare the Roblox client")) + ": " +
                           prepared.error,
                "", false};

    const Settings settings = loadSettings(installDir);
    // The graphics choice writes Windows renderer flags (D3D11/Vulkan/OpenGL);
    // the macOS client renders through Metal and ignores them. Only the
    // flags the user actually entered are carried over.
    const std::string flags = renderClientAppSettings(settings.fastFlags.player);
    const std::string flagError = macclient::writeAppSettings(prepared.build, flags);
    if (!flagError.empty())
        return {false, std::string(i18n::tr("could not write the Roblox settings")) + ": " +
                           flagError,
                "", false};

    std::vector<std::string> env = launchEnvVars(installDir, LaunchTarget::Player);
    for (const std::string& kv : flagEnvVars(installDir)) env.push_back(kv);
    env.insert(env.end(), extraEnv.begin(), extraEnv.end());
    env.push_back(std::string("ROSEMARY_PREFIX=") +
                  (fs::path(installDir) / "mac-client" / "prefix").string());
    env.push_back("ROSEMARY_PREFIX_SETUP=download");
    // The Player must see an Intel Mac so it downloads the x86-64 build the
    // native backend runs directly.
    env.push_back("ROSEMARY_PROC_TRANSLATED=0");
    env.push_back("SDL_APP_ID=net.tuxblox.TuxBlox.Player");
    // Marks this process as a macOS-client session so the window's session
    // census can find it without the Windows prefix's WINEPREFIX match.
    env.push_back("TUXBLOX_MAC_CLIENT=1");

    std::error_code ec;
    fs::create_directories(installDir + "/logs", ec);
    const std::string logPath = launchLogPath(installDir, LaunchTarget::Player);
    std::vector<std::string> argv{macclient::resolveRosemary(installDir)};
    const auto guestArgs = macclient::playerLaunchArgs(prepared.build.exe, uri);
    argv.insert(argv.end(), guestArgs.begin(), guestArgs.end());

    if (!p.start(argv, env, logPath))
        return {false, i18n::tr("failed to start the client process"), "", false};
    return {true, "", logPath, false};
}

LaunchOutcome ProcessLauncher::launch(LaunchTarget target, const std::string& uri,
                                       const std::vector<std::string>& extraEnv) {
    TrackedProcess& p = processFor(target);
    if (p.isRunning()) {
        return {false, i18n::tr("already running"), "", false};
    }
    if (target == LaunchTarget::Player && launcherFlags().macClient)
        return launchMacPlayer(installDir_, p, uri, extraEnv);

    std::string exePath = resolveActiveVersionExePath(target, installDir_);
    bool wasBootstrapInstall = false;
    if (exePath.empty()) {
        exePath = resolveOrBootstrapExePath(target, installDir_);
        // resolveOrBootstrapExePath falls back to the official installer
        // whenever resolveExePath's lnk lookup finds nothing -- detect that
        // case the same way it does internally (no exe found via lnk), so
        // watch_launch.cpp (Task 7) knows to register whatever version this
        // run produces.
        const std::string driveC = installDir_ + "/runtime/pfx/drive_c";
        wasBootstrapInstall = !exePath.empty() && resolveExePath(target, driveC).empty();
    }
    if (exePath.empty()) {
        return {false, i18n::tr("could not resolve or download the Roblox executable"), "", false};
    }

    // Applied in the child (via TrackedProcess::start's env overlay) rather
    // than here in the parent -- see Finding 6, 2026-07-28 final review:
    // setenv()/getenv() are not thread-safe in glibc, and this (parent)
    // process has other threads that may call getenv() concurrently.
    std::vector<std::string> env = launchEnvVars(installDir_, target);
    for (const std::string& kv : flagEnvVars(installDir_)) env.push_back(kv);
    env.insert(env.end(), extraEnv.begin(), extraEnv.end());

    // Multi-instance. A launch into a prefix that already has a live Roblox
    // process must NOT use Proton's "run" verb:
    //
    //  - "run" goes through init_session(update_prefix_files=True), which calls
    //    Prefix::setup() in compat/tuxblox/src/prefix/prefix.cpp. That rewrites prefix state --
    //    upgrade_pfx(), migrate_user_paths(), and sync_host_theme()'s direct
    //    user.reg text writes -- underneath a live wineserver, which owns the
    //    registry and flushes over such writes.
    //  - "run" ends in Session::waitForPrefixDrain() in compat/tuxblox/src/launch/session.cpp, whose deadline
    //    is held off for as long as ANY Roblox process holds the prefix
    //    (Session::run in compat/tuxblox/src/launch/session.cpp) -- including one
    //    belonging to a different instance.
    //    This launch's exit would not be reported until every instance exited,
    //    delaying its crash popup and Roblox-log capture indefinitely.
    //
    // "run --immediate" skips both: it is excluded from the prefix-setup gate
    // and waits on its own process only. Same finding the Studio MCP gateway
    // rests on -- it is used precisely while Studio is open.
    // (It replaces the old "runinprefix" verb, still accepted as a deprecated
    // alias by the launcher but warned about on every use.)
    //
    // Two launches racing from a cold prefix can both observe no holder and both
    // use "run". prefix_lock still serialises setup_prefix(), so nothing
    // corrupts; the only effect is that each drain-waits for the other, i.e.
    // today's single-instance behaviour. Rare and benign.
    // A live Player is left out of that count on purpose: the compatibility layer stands it down before the new client starts, so the drive is this launch's to set up.
    const bool secondary = launchJoinsPrefix(target, prefixSessions(installDir_ + "/runtime/pfx"));
    std::vector<std::string> argv = {compatBinaryPath(installDir_), "run"};
    if (secondary) argv.push_back("--immediate");
    if (allowRoot()) argv.push_back("--allow-root");
    argv.push_back(exePath);
    if (!uri.empty()) argv.push_back(uri);

    const std::string logsDir = installDir_ + "/logs";
    std::error_code ec;
    fs::create_directories(logsDir, ec);
    const std::string logPath = launchLogPath(installDir_, target);

    if (!p.start(argv, env, logPath)) {
        return {false, i18n::tr("failed to start Proton process"), "", false};
    }
    return {true, "", logPath, wasBootstrapInstall};
}

void ProcessLauncher::stop(LaunchTarget target) {
    processFor(target).stop();
}

std::optional<ExitEvent> ProcessLauncher::takeExitEvent(LaunchTarget target) {
    return processFor(target).takeExitEvent();
}

} // namespace tuxblox
