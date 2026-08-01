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
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>

static bool contains(const std::vector<std::string>& env, const std::string& kv) {
    return std::find(env.begin(), env.end(), kv) != env.end();
}

int main() {
    using namespace tuxblox;
    using namespace std::chrono_literals;

    assert(protonBinaryPath("/x/tuxblox") == "/x/tuxblox/ProtonBuild/dist/proton");

    // launchEnvVars(): Player gets the shared Proton vars, no DXVK_CONFIG override.
    {
        auto env = launchEnvVars("/x/tuxblox", LaunchTarget::Player);
        assert(contains(env, "STEAM_COMPAT_DATA_PATH=/x/tuxblox/runtime"));
        assert(contains(env, "STEAM_COMPAT_CLIENT_INSTALL_PATH=/x/tuxblox"));
        assert(contains(env, "PROTON_LOG_DIR=/x/tuxblox/logs"));
        assert(contains(env, "DXVK_ASYNC=1"));
        assert(!contains(env, "DXVK_CONFIG=dxgi.enableDummyCompositionSwapchain=True"));
    }

    // launchEnvVars(): Studio additionally gets the composition-swapchain override.
    {
        auto env = launchEnvVars("/x/tuxblox", LaunchTarget::Studio);
        assert(contains(env, "DXVK_CONFIG=dxgi.enableDummyCompositionSwapchain=True"));
    }

    // launchEnvVars() must not touch this process's own environment -- it's
    // meant to be applied only to a Proton child (via setenv() in a
    // soon-to-exec()'d child, or an explicit execve() envp), never to the
    // launcher process itself, which may go on to spawn other, non-Proton
    // children. See item 33 in plan/plan.txt.
    {
        assert(getenv("STEAM_COMPAT_DATA_PATH") == nullptr);
        (void)launchEnvVars("/x/tuxblox", LaunchTarget::Player);
        assert(getenv("STEAM_COMPAT_DATA_PATH") == nullptr);
    }

    // Short-lived process: exits on its own, poll() must detect that.
    {
        TrackedProcess p;
        assert(p.start({"sh", "-c", "exit 0"}));
        assert(p.isRunning());
        std::this_thread::sleep_for(200ms);
        p.poll();
        assert(!p.isRunning());
    }

    // Long-lived process with a real grandchild: stop() must reach the whole
    // process group, not just the direct child, and must actually terminate
    // the grandchild (verified via /proc, not just via the parent's exit).
    {
        namespace fs = std::filesystem;
        fs::path pidFile = fs::temp_directory_path() / "tuxblox_test_process_launcher_grandchild_pid";
        fs::remove(pidFile);

        TrackedProcess p;
        std::string cmd = "sleep 30 & echo $! > " + pidFile.string() + "; wait";
        assert(p.start({"sh", "-c", cmd}));
        assert(p.isRunning());

        // Wait for the grandchild's pid to actually be written.
        pid_t grandchildPid = -1;
        for (int i = 0; i < 50 && grandchildPid <= 0; ++i) {
            std::this_thread::sleep_for(50ms);
            std::ifstream in(pidFile);
            if (in) in >> grandchildPid;
        }
        assert(grandchildPid > 0);
        assert(fs::exists("/proc/" + std::to_string(grandchildPid)));

        p.stop();
        std::this_thread::sleep_for(2500ms); // stop() now has its own ~2s SIGTERM grace period, see Finding 2
        p.poll();
        assert(!p.isRunning());
        assert(!fs::exists("/proc/" + std::to_string(grandchildPid))); // grandchild actually reaped too

        fs::remove(pidFile);
    }

    // start() while already running is a no-op (returns false, does not
    // replace the tracked pid).
    {
        TrackedProcess p;
        assert(p.start({"sleep", "5"}));
        assert(p.isRunning());
        bool second = p.start({"sleep", "5"});
        assert(!second);
        p.stop();
        std::this_thread::sleep_for(300ms);
        p.poll();
        assert(!p.isRunning());
    }

    // ProcessLauncher: Player and Studio are tracked independently.
    {
        ProcessLauncher launcher("/nonexistent/tuxblox_test_install_dir");
        assert(!launcher.pollIsRunning(LaunchTarget::Player));
        assert(!launcher.pollIsRunning(LaunchTarget::Studio));
        launcher.stop(LaunchTarget::Player); // no-op, must not throw/crash
        launcher.stop(LaunchTarget::Studio);
    }

    // exitCodeTitle(): a sample of the table plus the unknown-code fallback.
    {
        assert(std::string(exitCodeTitle(0)) == "OK");
        assert(std::string(exitCodeTitle(1)) == "General Error");
        assert(std::string(exitCodeTitle(139)) == "Segmentation Fault");
        assert(std::string(exitCodeTitle(187)) == "Session terminated by Hyperion");
        assert(std::string(exitCodeTitle(201)) == "Integrity verification failure");
        assert(std::string(exitCodeTitle(999999)) == "Unknown exit code");
    }

    // takeExitEvent(): a process that exits on its own reports its real
    // exit code and stopRequested == false.
    {
        TrackedProcess p;
        assert(p.start({"sh", "-c", "exit 7"}));
        std::this_thread::sleep_for(200ms);
        p.poll();
        assert(!p.isRunning());
        auto ev = p.takeExitEvent();
        assert(ev.has_value());
        assert(ev->exitCode == 7);
        assert(!ev->stopRequested);
        // Reported exactly once -- a second call after no new exit finds nothing.
        assert(!p.takeExitEvent().has_value());
    }

    // takeExitEvent(): stop() sets stopRequested == true, even though the
    // resulting exit code (from SIGTERM) is non-zero -- callers rely on
    // this to avoid treating a deliberate Stop as a crash.
    {
        TrackedProcess p;
        assert(p.start({"sleep", "30"}));
        assert(p.isRunning());
        p.stop();
        std::this_thread::sleep_for(2500ms);
        p.poll();
        assert(!p.isRunning());
        auto ev = p.takeExitEvent();
        assert(ev.has_value());
        assert(ev->exitCode != 0);
        assert(ev->stopRequested);
    }

    // start()'s logFilePath redirects the child's stdout/stderr.
    {
        namespace fs = std::filesystem;
        fs::path logPath = fs::temp_directory_path() / "tuxblox_test_process_launcher_log.txt";
        fs::remove(logPath);

        TrackedProcess p;
        assert(p.start({"sh", "-c", "echo hello-stdout; echo hello-stderr 1>&2"}, {}, logPath.string()));
        std::this_thread::sleep_for(200ms);
        p.poll();
        assert(!p.isRunning());

        std::ifstream in(logPath);
        assert(in);
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(contents.find("hello-stdout") != std::string::npos);
        assert(contents.find("hello-stderr") != std::string::npos);

        fs::remove(logPath);
    }

    printf("process_launcher: all tests passed\n");
    return 0;
}
