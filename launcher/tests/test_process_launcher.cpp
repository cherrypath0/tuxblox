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
#include "versions_manifest.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unistd.h>

namespace fs = std::filesystem;

static bool contains(const std::vector<std::string>& env, const std::string& kv) {
    return std::find(env.begin(), env.end(), kv) != env.end();
}

int main() {
    using namespace tuxblox;
    using namespace std::chrono_literals;

    assert(protonBinaryPath("/x/tuxblox") == "/x/tuxblox/proton/main");

    // launchEnvVars(): Player gets the shared Proton vars, no DXVK_CONFIG override.
    {
        auto env = launchEnvVars("/x/tuxblox", LaunchTarget::Player);
        assert(contains(env, "TUXBLOX_PREFIX=/x/tuxblox/runtime"));
        assert(contains(env, "PROTON_LOG_DIR=/x/tuxblox/logs"));
        // DXVK_ASYNC is deliberately absent: the bundled DXVK is 3.0.1, which
        // dropped async pipeline compilation, so setting it did nothing. Assert
        // it stays gone rather than silently creeping back in.
        assert(!contains(env, "DXVK_ASYNC=1"));
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
        assert(getenv("TUXBLOX_PREFIX") == nullptr);
        (void)launchEnvVars("/x/tuxblox", LaunchTarget::Player);
        assert(getenv("TUXBLOX_PREFIX") == nullptr);
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

    // exitCodeTitle(): a sample of the table plus the unknown-code fallback --
    // nullptr, not a made-up "Unknown exit code" string (see app.cpp's
    // handleUnexpectedExit: an undocumented code shows as a bare "Exit Code:
    // N", no invented description).
    {
        assert(std::string(exitCodeTitle(0)) == "OK");
        assert(std::string(exitCodeTitle(1)) == "General Error");
        assert(std::string(exitCodeTitle(139)) == "Segmentation Fault");
        assert(std::string(exitCodeTitle(187)) == "Session terminated by Hyperion");
        assert(std::string(exitCodeTitle(201)) == "Integrity verification failure");
        assert(exitCodeTitle(999999) == nullptr);
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

    // resolveOrBootstrapExePath(): a pre-existing cached installer (living
    // outside the prefix, e.g. installDir/RobloxPlayer/RobloxPlayerInstaller.exe)
    // must be staged into runtime/pfx/drive_c/TuxBloxStaging before use --
    // launching it via its raw host path silently fails post-Z:-removal
    // (observed as exit code 245). See plan/plan.txt item 20.
    {
        namespace fs = std::filesystem;
        fs::path installDir = fs::temp_directory_path() / "tuxblox_test_stage_install_dir";
        fs::remove_all(installDir);
        fs::create_directories(installDir / "RobloxPlayer");
        {
            std::ofstream out(installDir / "RobloxPlayer" / "RobloxPlayerInstaller.exe");
            out << "fake installer";
        }

        std::string exePath = resolveOrBootstrapExePath(LaunchTarget::Player, installDir.string());
        fs::path expected = installDir / "runtime" / "pfx" / "drive_c" / "TuxBloxStaging" / "RobloxPlayerInstaller.exe";
        assert(exePath == expected.string());
        assert(fs::exists(expected));

        fs::remove_all(installDir);
    }

    // findRealExitCodeInLog(): picks up Proton's relayed
    // "TUXBLOX_REAL_EXIT_CODE=<n>" marker line, taking the last occurrence
    // if there are several (e.g. a stale line from an earlier launch that
    // appended to the same log).
    {
        namespace fs = std::filesystem;
        fs::path logPath = fs::temp_directory_path() / "tuxblox_test_real_exit_code_log.txt";
        {
            std::ofstream out(logPath);
            out << "some proton startup noise\n";
            out << "TUXBLOX_REAL_EXIT_CODE=-2147467260\n";
            out << "more noise\n";
        }
        auto real = findRealExitCodeInLog(logPath.string());
        assert(real.has_value());
        assert(*real == -2147467260);
        fs::remove(logPath);
    }

    // findRealExitCodeInLog(): no marker present, or no log at all -> nullopt.
    {
        namespace fs = std::filesystem;
        fs::path logPath = fs::temp_directory_path() / "tuxblox_test_real_exit_code_log_empty.txt";
        {
            std::ofstream out(logPath);
            out << "nothing relevant here\n";
        }
        assert(!findRealExitCodeInLog(logPath.string()).has_value());
        assert(!findRealExitCodeInLog("/nonexistent/tuxblox_test_no_such_log.txt").has_value());
        fs::remove(logPath);
    }

    // resolveActiveVersionExePath(): no versions.json -> empty (nothing pinned).
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_process_launcher_activepin";
        fs::remove_all(dir);
        fs::create_directories(dir);
        assert(resolveActiveVersionExePath(LaunchTarget::Player, dir.string()).empty());
        fs::remove_all(dir);
    }

    // resolveActiveVersionExePath(): active hash set but the exe file
    // doesn't actually exist on disk -> empty (never point at a
    // nonexistent path; the caller falls back to the existing
    // lnk/bootstrap path instead of failing outright).
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_process_launcher_activepin2";
        fs::remove_all(dir);
        fs::create_directories(dir);
        VersionsManifest m;
        m.player.activeHash = "version-doesnotexist";
        saveVersionsManifest(dir.string(), m);
        assert(resolveActiveVersionExePath(LaunchTarget::Player, dir.string()).empty());
        fs::remove_all(dir);
    }

    // resolveActiveVersionExePath(): active hash set AND the exe exists ->
    // returns that exact path.
    {
        namespace fs = std::filesystem;
        fs::path dir = fs::temp_directory_path() / "tuxblox_test_process_launcher_activepin3";
        fs::remove_all(dir);
        fs::path versionDir = dir / "runtime/pfx/drive_c/users/user/AppData/Local/Roblox/Versions/version-abc123";
        fs::create_directories(versionDir);
        std::ofstream exeStub(versionDir / "RobloxPlayerBeta.exe");
        exeStub << "stub";
        exeStub.close();

        VersionsManifest m;
        m.player.activeHash = "version-abc123";
        saveVersionsManifest(dir.string(), m);

        std::string resolved = resolveActiveVersionExePath(LaunchTarget::Player, dir.string());
        assert(resolved == (versionDir / "RobloxPlayerBeta.exe").string());
        fs::remove_all(dir);
    }

    // Multi-instance: two launches from the same second must not share a log
    // file. Both would dup2() into it and interleave their output.
    {
        const fs::path tmp = fs::temp_directory_path() / "tuxblox_launch_logname_test";
        fs::remove_all(tmp);
        fs::create_directories(tmp / "logs");

        // launchLogPath() is the seam under test -- it must vary per process.
        const std::string a = launchLogPath(tmp.string(), LaunchTarget::Studio);
        assert(a.find("/logs/RobloxStudio-") != std::string::npos);
        assert(a.rfind(".log") == a.size() - 4);
        // The pid is the uniquifier, so the same process gets the same name and
        // a different process would not. Assert the pid is actually present.
        assert(a.find("-" + std::to_string(getpid()) + ".log") != std::string::npos);

        const std::string p = launchLogPath(tmp.string(), LaunchTarget::Player);
        assert(p.find("/logs/RobloxPlayer-") != std::string::npos);

        fs::remove_all(tmp);
    }

    printf("process_launcher: all tests passed\n");
    return 0;
}
