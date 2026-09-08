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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include "session.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <string_view>

#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Only these keep a prefix session alive. The crash handler, StudioMCP and
// RCCService are helpers: if they are all that is left, the session is over
// and the prefix should be torn down.
const std::array<std::string_view, 2> ClientHolderImages = {
    "robloxplayerbeta.exe",
    "robloxstudiobeta.exe"
};

const std::array<std::string_view, 2> InstallerHolderImages = {
    "robloxplayerinstaller.exe",
    "robloxstudioinstaller.exe"
};

// How long an installer may keep running after the client it installed has
// started. It normally exits within a couple of seconds, so anything past
// this is worth telling the user about rather than waiting on in silence.
const int StuckInstallerNoticeSeconds = 15;

std::string toLower(const std::string& text) {
    std::string lowered = text;
    std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lowered;
}

bool imageIsClient(const std::string& image) {
    const std::string lowered = toLower(image);
    return std::find(ClientHolderImages.begin(), ClientHolderImages.end(), lowered) !=
           ClientHolderImages.end();
}

bool imageIsInstaller(const std::string& image) {
    const std::string lowered = toLower(image);
    return std::find(InstallerHolderImages.begin(), InstallerHolderImages.end(), lowered) !=
           InstallerHolderImages.end();
}

// The bare file name of a path, for matching against the image lists above.
std::string imageNameOf(const std::string& path) {
    std::string name = path;
    std::replace(name.begin(), name.end(), '\\', '/');
    const size_t slash = name.rfind('/');
    return slash == std::string::npos ? name : name.substr(slash + 1);
}

std::string describeSessionHolders(const std::vector<SessionHolder>& holders) {
    std::string description;
    for (const SessionHolder& holder : holders) {
        if (!description.empty()) {
            description += ", ";
        }
        description += holder.image + " (pid " + holder.pid + ")";
    }
    return description;
}

std::string envOrEmpty(const char *pName) {
    const char *pValue = std::getenv(pName);
    return pValue != nullptr ? std::string(pValue) : std::string();
}

bool envIsOn(const char *pName) {
    return nonzero(envOrEmpty(pName));
}

// execve wants a NUL-terminated array of "KEY=VALUE" pointers.
std::vector<char *> buildEnvArray(const Environment& env, std::vector<std::string>& storage) {
    storage.clear();
    storage.reserve(env.size());
    for (const auto& [key, value] : env) {
        storage.push_back(key + "=" + value);
    }
    std::vector<char *> pointers;
    pointers.reserve(storage.size() + 1);
    for (std::string& entry : storage) {
        pointers.push_back(entry.data());
    }
    pointers.push_back(nullptr);
    return pointers;
}

// Runs a command to completion with no signal handling of its own. Used for
// the wineserver teardown calls, which must not re-enter the signal path.
int runSimple(const std::vector<std::string>& command, const Environment& localEnv, int logFd);

std::vector<char *> buildArgArray(const std::vector<std::string>& command,
                                  std::vector<std::string>& storage) {
    storage = command;
    std::vector<char *> pointers;
    pointers.reserve(storage.size() + 1);
    for (std::string& entry : storage) {
        pointers.push_back(entry.data());
    }
    pointers.push_back(nullptr);
    return pointers;
}

// A Wine process's /proc/<pid>/cmdline holds the Windows command line, so the
// image name is what identifies it. comm is no use: Studio names its main
// thread "Main".
std::string pidWineImage(const std::string& pid) {
    std::ifstream cmdline("/proc/" + pid + "/cmdline", std::ios::binary);
    if (!cmdline) {
        return "";
    }
    std::string first;
    std::getline(cmdline, first, '\0');

    // Cut at the first ".exe" rather than the first space: the image path can
    // contain spaces, and later arguments can contain further ".exe" paths.
    const size_t cut = toLower(first).find(".exe");
    if (cut == std::string::npos) {
        return "";
    }
    return imageNameOf(first.substr(0, cut + 4));
}

std::string pidWinePrefix(const std::string& pid) {
    std::ifstream environ("/proc/" + pid + "/environ", std::ios::binary);
    if (!environ) {
        // Not ours to read (another user, a kernel thread), so not ours.
        return "";
    }
    std::string entry;
    const std::string wanted = "WINEPREFIX=";
    while (std::getline(environ, entry, '\0')) {
        if (entry.compare(0, wanted.size(), wanted) == 0) {
            std::error_code error;
            const fs::path value = entry.substr(wanted.size());
            const fs::path normalized = value.lexically_normal();
            return normalized.string();
        }
    }
    return "";
}

int runSimple(const std::vector<std::string>& command, const Environment& localEnv, int logFd) {
    const pid_t child = ::fork();
    if (child < 0) {
        return -1;
    }
    if (child == 0) {
        if (logFd >= 0) {
            ::dup2(logFd, STDOUT_FILENO);
            ::dup2(logFd, STDERR_FILENO);
        }
        std::vector<std::string> argStorage;
        std::vector<std::string> envStorage;
        std::vector<char *> argv = buildArgArray(command, argStorage);
        std::vector<char *> envp = buildEnvArray(localEnv, envStorage);
        ::execve(argv[0], argv.data(), envp.data());
        ::_exit(127);
    }
    int status = 0;
    while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

// A waitpid() status turned into the single number the launcher shows. A
// process killed by a signal reports as 128 + the signal, matching what a
// shell would report for the same death.
int exitCodeFromStatus(int status) {
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1;
}

} // namespace

std::vector<SessionHolder> prefixSessionHolders(const fs::path& prefixDir) {
    std::vector<SessionHolder> holders;
    const std::string wanted = prefixDir.lexically_normal().string();

    std::error_code error;
    fs::directory_iterator procEntries("/proc", error);
    if (error) {
        return holders;
    }

    for (const fs::directory_entry& entry : procEntries) {
        const std::string pid = entry.path().filename().string();
        if (pid.empty() || !std::all_of(pid.begin(), pid.end(),
                                        [](unsigned char c) { return std::isdigit(c); })) {
            continue;
        }
        // Image name first: cmdline is world-readable and cheap, and it
        // narrows a few hundred processes down to the handful worth reading
        // environ for.
        const std::string image = pidWineImage(pid);
        const bool client = imageIsClient(image);
        if (!client && !imageIsInstaller(image)) {
            continue;
        }
        if (pidWinePrefix(pid) != wanted) {
            continue;
        }
        SessionHolder holder;
        holder.pid = pid;
        holder.image = image;
        holder.client = client;
        holders.push_back(holder);
    }
    return holders;
}

Session::Session(Proton& protonDist, fs::path prefix)
    : proton(protonDist), prefixDir(std::move(prefix)) {
    for (char **ppEntry = environ; ppEntry != nullptr && *ppEntry != nullptr; ppEntry++) {
        const std::string entry = *ppEntry;
        const size_t equals = entry.find('=');
        if (equals != std::string::npos) {
            env[entry.substr(0, equals)] = entry.substr(equals + 1);
        }
    }

    // winebth.sys crashes winedevice.exe, and Roblox has no use for Bluetooth.
    dllOverrides["winebth.sys"] = "d";
    dllOverrides["opencl"] = "n,d";

    // The layer ships neither Gecko nor Mono, so leaving these enabled would
    // only ever produce a download prompt nobody can act on. Roblox's in-client
    // browser is WebView2, which webview2loader answers, not mshtml.
    dllOverrides["mshtml"] = "d";
    dllOverrides["mscoree"] = "d";
}

Session::~Session() {
    if (logFd >= 0) {
        ::close(logFd);
    }
}

void Session::initWine() {
    env["WINEPREFIX"] = prefixDir.string();
    env.erase("LC_ALL");
    env.erase("WINEARCH");

    // Let Wine restore the original value when it launches an external app.
    if (env.find("ORIG_LD_LIBRARY_PATH") == env.end()) {
        env["ORIG_LD_LIBRARY_PATH"] = envOrEmpty("LD_LIBRARY_PATH");
    }

    const std::string libDir = proton.libDir.string() + "/";
    const std::vector<std::string> libraryPaths = {
        libDir + "x86_64-linux-gnu",
        libDir + "i386-linux-gnu"
    };

    std::string joinedLibs;
    for (const std::string& entry : libraryPaths) {
        joinedLibs += (joinedLibs.empty() ? "" : ":") + entry;
    }
    prependToEnvStr(env, "LD_LIBRARY_PATH", joinedLibs, ":");

    std::string dllPaths = libDir + "vkd3d:" + libDir + "wine";
    const std::string inheritedDllPath = envOrEmpty("WINEDLLPATH");
    if (!inheritedDllPath.empty()) {
        dllPaths += ":" + inheritedDllPath;
    }
    env["WINEDLLPATH"] = dllPaths;

    std::string gstPaths;
    for (const std::string& entry : libraryPaths) {
        gstPaths += (gstPaths.empty() ? "" : ":") + entry + "/gstreamer-1.0";
    }
    env["GST_PLUGIN_SYSTEM_PATH_1_0"] = gstPaths;
    env["WINE_GST_REGISTRY_DIR"] = (prefixDir.parent_path() / "gstreamer-1.0").string() + "/";

    // Root of the WebKitGTK bundle. webview2loader derives every other
    // WebKit path from this one value, so nothing else needs setting here.
    env["TUXBLOX_WEBVIEW_DIR"] = libDir + "tuxblox-webview/";

    prependToEnvStr(env, "PATH", proton.binDir.string(), ":");
}

void Session::initSession() {
    const bool logging = envIsOn("TUXBLOX_LOG");

    if (logging) {
        const std::string requested = envOrEmpty("TUXBLOX_LOG");
        if (env.find("WINEDEBUG") == env.end()) {
            env["WINEDEBUG"] = "+timestamp,+pid,+tid,+seh,+unwind,+threadname,"
                               "+debugstr,+loaddll,+mscoree";
        }
        if (requested != "1") {
            appendToEnvStr(env, "WINEDEBUG", requested, ",");
        }
        env.emplace("DXVK_LOG_LEVEL", "info");
        env.emplace("VKD3D_DEBUG", "warn");
        env.emplace("VKD3D_SHADER_DEBUG", "fixme");
        env.emplace("WINE_MONO_TRACE", "E:System.NotImplementedException");
    }

    // Logging is off by default: it costs real frame time.
    env.emplace("WINEDEBUG", "-all");
    env.emplace("DXVK_LOG_LEVEL", "none");
    env.emplace("VKD3D_DEBUG", "none");
    env.emplace("VKD3D_SHADER_DEBUG", "none");

    // Synchronization ladder is ntsync, then fsync, then wineserver. Wine
    // picks the best one available; TUXBLOX_NO_FSYNC drops it a rung.
    if (envIsOn("TUXBLOX_NO_FSYNC")) {
        env.erase("WINEFSYNC");
    } else {
        env["WINEFSYNC"] = "1";
    }

    // Roblox is 32-bit-aware but benefits from the full address space.
    env["WINE_LARGE_ADDRESS_AWARE"] = "1";

    env.emplace("__GLVND_DISALLOW_PATCHING", "1");
    env.emplace("WINE_MONO_HIDETYPES", "0");
    env.emplace("PROTON_USE_XALIA", "0");

    // Graphics backend selection. Roblox picks its own renderer at runtime and
    // does not always land on Vulkan, so the Direct3D path has to be set up
    // and working even when a given session never touches it.
    graphics.useWineD3D = envIsOn("TUXBLOX_USE_WINED3D");
    graphics.noD3D11 = envIsOn("TUXBLOX_NO_D3D11");

    const auto existingOverrides = env.find("WINEDLLOVERRIDES");
    const bool dxgiOverridden = existingOverrides != env.end() &&
                                existingOverrides->second.find("dxgi=b") != std::string::npos;
    graphics.useDxvkDxgi = !graphics.useWineD3D && !dxgiOverridden;

    graphics.useNvapi = !envIsOn("TUXBLOX_DISABLE_NVAPI");
    if (graphics.useNvapi) {
        env["DXVK_ENABLE_NVAPI"] = "1";
    }

    if (graphics.noD3D11) {
        dllOverrides["d3d11"] = "";
        dllOverrides.erase("dxgi");
    }

    if (logging) {
        openLogFile();
    }
}

void Session::applyDllOverrides() {
    std::string overrides;
    for (const auto& [dll, setting] : dllOverrides) {
        overrides += (overrides.empty() ? "" : ";") + dll + "=" + setting;
    }
    appendToEnvStr(env, "WINEDLLOVERRIDES", overrides, ";");
}

void Session::openLogFile() {
    const std::string logDir = envOrEmpty("TUXBLOX_LOG_DIR");
    const fs::path base = logDir.empty() ? fs::path(envOrEmpty("HOME")) : fs::path(logDir);
    makeDirs(base);

    logPath = base / "tuxblox.log";
    std::error_code error;
    fs::remove(logPath, error);

    logFd = ::open(logPath.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (logFd < 0) {
        log("Could not open log file \"" + logPath.string() + "\": " + std::strerror(errno));
        env["WINEDEBUG"] = "-all";
    }
}

void Session::writeLogHeader(const std::vector<std::string>& target) {
    if (logFd < 0) {
        return;
    }

    std::string header = "======================\n";
    header += "TuxBlox: " + buildVersion + "\n";
    header += "Launched as: " + invocation + "\n";
    header += "Command: " + joinCommandLine(target) + "\n";

    struct utsname info = {};
    if (::uname(&info) == 0) {
        header += std::string("Kernel: ") + info.sysname + " " + info.release + " " +
                  info.version + " " + info.machine + "\n";
    }
    header += "WINEDEBUG: " + env["WINEDEBUG"] + "\n";
    header += "WINEDLLOVERRIDES: " + env["WINEDLLOVERRIDES"] + "\n";
    header += "======================\n";

    ssize_t written = ::write(logFd, header.data(), header.size());
    static_cast<void>(written);
}

int Session::runProc(const std::vector<std::string>& command) {
    return runProc(command, env);
}

int Session::runProc(const std::vector<std::string>& command, const Environment& localEnv) {
    // Block both signals up front so the wait loop can collect them with
    // sigtimedwait instead of running teardown inside a signal handler,
    // where almost nothing is safe to call.
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);

    sigset_t previous;
    sigprocmask(SIG_BLOCK, &blocked, &previous);

    const pid_t child = ::fork();
    if (child < 0) {
        sigprocmask(SIG_SETMASK, &previous, nullptr);
        log("Could not start " + command.front() + ": " + std::strerror(errno));
        return -1;
    }

    if (child == 0) {
        // Its own process group, so a Ctrl+C takes the whole tree down --
        // wine, the game, and helpers like WebView2's crashpad handler --
        // instead of leaving them orphaned once this process exits.
        ::setsid();

        if (logFd >= 0) {
            ::dup2(logFd, STDOUT_FILENO);
            ::dup2(logFd, STDERR_FILENO);
        }

        // Start inside drive_c rather than wherever the launcher was invoked
        // from. Without a Z: drive a unix path outside C: can only be spelled
        // via ntdll's \??\unix\ form, and RtlSetCurrentDirectory_U corrupts
        // that when storing it, leaking a malformed path into every child.
        const fs::path driveC = prefixDir / "drive_c";
        if (::chdir(driveC.c_str()) != 0) {
            ::_exit(127);
        }

        sigprocmask(SIG_SETMASK, &previous, nullptr);

        std::vector<std::string> argStorage;
        std::vector<std::string> envStorage;
        std::vector<char *> argv = buildArgArray(command, argStorage);
        std::vector<char *> envp = buildEnvArray(localEnv, envStorage);
        ::execve(argv[0], argv.data(), envp.data());
        ::_exit(127);
    }

    int status = 0;
    const struct timespec pollInterval = {0, 100 * 1000 * 1000};
    time_t clientStartedAt = 0;
    time_t lastHolderCheck = 0;
    bool reportedStuckTarget = false;

    while (true) {
        // An installer hands over to the client it installed and exits within
        // a couple of seconds. One that is still here well after the client
        // started has stopped making progress, and this wait would otherwise
        // sit on it in silence.
        const time_t now = ::time(nullptr);
        if (targetIsInstaller && !reportedStuckTarget && now != lastHolderCheck) {
            lastHolderCheck = now;
            if (clientStartedAt == 0) {
                for (const SessionHolder& holder : prefixSessionHolders(prefixDir)) {
                    if (holder.client) {
                        clientStartedAt = now;
                        break;
                    }
                }
            } else if (now - clientStartedAt >= StuckInstallerNoticeSeconds) {
                reportedStuckTarget = true;
                log("The Roblox installer has not exited " +
                    std::to_string(StuckInstallerNoticeSeconds) +
                    "s after Roblox started, so it may be stuck. Press Ctrl+C to "
                    "tear the prefix down.");
            }
        }

        const int signalNumber = ::sigtimedwait(&blocked, nullptr, &pollInterval);
        if (signalNumber == SIGINT || signalNumber == SIGTERM) {
            killProcessGroup(child, SIGTERM);

            bool reaped = false;
            for (int attempt = 0; attempt < 50; attempt++) {
                if (::waitpid(child, &status, WNOHANG) != 0) {
                    reaped = true;
                    break;
                }
                ::nanosleep(&pollInterval, nullptr);
            }
            if (!reaped) {
                killProcessGroup(child, SIGKILL);
                ::waitpid(child, &status, 0);
            }

            // wineserver outlives any single client on purpose, so it and the
            // helpers connected to it survive killing our own process group.
            // Tear the prefix down explicitly.
            runSimple({proton.wineserverBin.string(), "-k"}, localEnv, logFd);

            sigprocmask(SIG_SETMASK, &previous, nullptr);
            reportExitCodes(exitCodeFromStatus(status));
            // A user-initiated Ctrl+C is not a TuxBlox failure, so this
            // reports as "the wrapped process ended abnormally".
            std::exit(2);
        }

        const pid_t finished = ::waitpid(child, &status, WNOHANG);
        if (finished == child) {
            break;
        }
        if (finished < 0 && errno != EINTR) {
            break;
        }
    }

    sigprocmask(SIG_SETMASK, &previous, nullptr);

    const int exitCode = exitCodeFromStatus(status);
    reportExitCodes(exitCode);
    return exitCode;
}

void Session::reportExitCodes(int exitCode) {
    // This process's own exit code is a fixed 0/1/2 (see main.cpp), so both
    // real numbers have to reach the launcher some other way: as marker lines
    // on this process's stderr, which the launcher captures.
    //
    // TUXBLOX_REAL_EXIT_CODE is the wrapped process's own, non-truncated
    // Windows exit code, written by ntdll to the wrapped process's stderr --
    // which is the log file, not this process's stderr, whenever logging is
    // on. Carry it across that boundary here.
    if (!logPath.empty()) {
        std::ifstream logIn(logPath);
        if (logIn) {
            std::string line;
            std::string found;
            while (std::getline(logIn, line)) {
                if (line.compare(0, 23, "TUXBLOX_REAL_EXIT_CODE=") == 0) {
                    found = line;
                }
            }
            if (!found.empty()) {
                while (!found.empty() && (found.back() == '\r' || found.back() == '\n')) {
                    found.pop_back();
                }
                writeLine(found);
            }
        }
    }

    // TUXBLOX_WRAPPER_EXIT_CODE is what waitpid() reported for the process we
    // started. It is always available, unlike the line above -- ntdll only
    // reports for the three target images, and only when the process gets far
    // enough to terminate itself rather than being killed outright. The
    // launcher prefers the real code and falls back to this one, so a crash
    // popup always has an actual number to show rather than a bare "2".
    if (exitCode != 0) {
        writeLine("TUXBLOX_WRAPPER_EXIT_CODE=" + std::to_string(exitCode));
    }
}

void Session::writeLine(const std::string& line) {
    const std::string out = line + "\n";
    ssize_t written = ::write(STDERR_FILENO, out.data(), out.size());
    static_cast<void>(written);
}

void Session::waitForPrefixDrain(int timeoutSeconds) {
    std::vector<std::string> argStorage;
    std::vector<std::string> envStorage;

    // Blocked here too, not just in runProc: a Ctrl+C while waiting has to tear
    // the prefix down, otherwise wineserver and everything still connected to
    // it outlive this process as orphans.
    sigset_t blocked;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);

    sigset_t previous;
    sigprocmask(SIG_BLOCK, &blocked, &previous);

    const pid_t watcher = ::fork();
    if (watcher < 0) {
        sigprocmask(SIG_SETMASK, &previous, nullptr);
        return;
    }
    if (watcher == 0) {
        ::setsid();
        sigprocmask(SIG_SETMASK, &previous, nullptr);
        if (logFd >= 0) {
            ::dup2(logFd, STDOUT_FILENO);
            ::dup2(logFd, STDERR_FILENO);
        }
        const std::vector<std::string> command = {proton.wineserverBin.string(), "-w"};
        std::vector<char *> argv = buildArgArray(command, argStorage);
        std::vector<char *> envp = buildEnvArray(env, envStorage);
        ::execve(argv[0], argv.data(), envp.data());
        ::_exit(127);
    }

    // The timeout applies only once nothing but helper processes are left. For
    // as long as a real Roblox process is connected the deadline is held off:
    // "still running after 15s" is what a working launch looks like, and the
    // self-relaunch this wait exists for is exactly an app outliving the
    // process runProc waited on.
    bool haveDeadline = false;
    time_t deadline = 0;
    std::string reported;
    const struct timespec pollInterval = {1, 0};

    while (true) {
        int status = 0;
        if (::waitpid(watcher, &status, WNOHANG) == watcher) {
            sigprocmask(SIG_SETMASK, &previous, nullptr);
            return;
        }

        const int signalNumber = ::sigtimedwait(&blocked, nullptr, &pollInterval);
        if (signalNumber == SIGINT || signalNumber == SIGTERM) {
            log("Interrupted while waiting for the prefix, tearing it down");
            break;
        }

        const std::vector<SessionHolder> holders = prefixSessionHolders(prefixDir);
        if (!holders.empty()) {
            // Naming what the wait is on, so an app that never exits reads as
            // a stuck process rather than as a launcher that hung.
            const std::string description = describeSessionHolders(holders);
            if (description != reported) {
                reported = description;
                log("Waiting for " + description +
                    " to close. Press Ctrl+C to close it and tear the prefix down.");
            }
            haveDeadline = false;
        } else if (!haveDeadline) {
            haveDeadline = true;
            deadline = ::time(nullptr) + timeoutSeconds;
        } else if (::time(nullptr) >= deadline) {
            log("Prefix still busy " + std::to_string(timeoutSeconds) +
                "s after the last app exited, tearing it down");
            break;
        }
    }

    runSimple({proton.wineserverBin.string(), "-k"}, env, logFd);

    for (int attempt = 0; attempt < 50; attempt++) {
        int status = 0;
        if (::waitpid(watcher, &status, WNOHANG) == watcher) {
            sigprocmask(SIG_SETMASK, &previous, nullptr);
            return;
        }
        const struct timespec shortWait = {0, 100 * 1000 * 1000};
        ::nanosleep(&shortWait, nullptr);
    }
    ::kill(watcher, SIGKILL);
    int status = 0;
    ::waitpid(watcher, &status, 0);
    sigprocmask(SIG_SETMASK, &previous, nullptr);
}

int Session::run(const std::vector<std::string>& target) {
    writeLogHeader(target);

    targetIsInstaller = !target.empty() && imageIsInstaller(imageNameOf(target.front()));

    // Run through the preloader directly rather than restarting through
    // start.exe.
    env["WINELOADERNOEXEC"] = "1";
    const std::string unixDir = proton.libDir.string() + "/wine/x86_64-unix/";

    std::vector<std::string> command = {unixDir + "wine-preloader", unixDir + "wine"};
    command.insert(command.end(), target.begin(), target.end());

    return runProc(command);
}

} // namespace tuxblox
