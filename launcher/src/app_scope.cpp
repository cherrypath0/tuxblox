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

#include "app_scope.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

namespace tuxblox {

namespace {

// The desktop entry ensureDesktopIntegration() writes as
// tuxblox-launcher.desktop -- the one carrying Name=TuxBlox and Icon=tuxblox,
// which is exactly what we want a shell to display for a running session.
constexpr const char* kDesktopId = "tuxblox-launcher";

std::string readSelfCgroup() {
    std::ifstream f("/proc/self/cgroup");
    if (!f) return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Same shape as desktop_integration.cpp's best-effort runner, and for the
// same reasons: build the argv array before fork() so nothing allocates
// between fork and exec, and bound the wait so a wedged D-Bus can't hang
// startup.
void runBestEffort(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        // Silence it: a refused or unavailable systemd is an expected outcome
        // here, not something worth printing over the launcher's own output.
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) close(devnull);
        }
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

} // namespace

std::string escapeUnitName(const std::string& desktopId) {
    std::string out;
    out.reserve(desktopId.size());
    for (char c : desktopId) {
        if (c == '-') out += "\\x2d";
        else out += c;
    }
    return out;
}

std::string appScopeUnitName(const std::string& desktopId, int pid) {
    return "app-" + escapeUnitName(desktopId) + "-" + std::to_string(pid) + ".scope";
}

bool alreadyInAppScope(const std::string& cgroupLine, const std::string& desktopId) {
    // Match on "app-<escaped id>" rather than the whole unit name: the random
    // suffix differs per launch, and an inherited scope (the --watch-launch
    // helper picking up the GUI's) carries the *parent's* pid in that suffix.
    return cgroupLine.find("app-" + escapeUnitName(desktopId)) != std::string::npos;
}

void ensureAppScope() {
    const std::string cgroup = readSelfCgroup();
    // No cgroup line at all means no systemd-managed session to join.
    if (cgroup.empty()) return;
    if (alreadyInAppScope(cgroup, kDesktopId)) return;

    const int pid = static_cast<int>(getpid());
    const std::string unit = appScopeUnitName(kDesktopId, pid);
    const std::string pidStr = std::to_string(pid);

    // StartTransientUnit(name, mode, properties, aux). A *scope* is the unit
    // type that adopts already-running pids, which is what we need -- a
    // transient service would have to spawn something instead.
    //
    // CollectMode=inactive-or-failed so systemd garbage-collects the unit if
    // it ever fails, rather than leaving a failed unit behind for the user to
    // reset by hand.
    runBestEffort({
        "busctl", "--user", "call",
        "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager", "StartTransientUnit",
        "ssa(sv)a(sa(sv))",
        unit, "fail",
        "2",
        "PIDs", "au", "1", pidStr,
        "CollectMode", "s", "inactive-or-failed",
        "0",
    });
}

} // namespace tuxblox
