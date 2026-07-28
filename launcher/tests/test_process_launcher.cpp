#include "process_launcher.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>

int main() {
    using namespace tuxblox;
    using namespace std::chrono_literals;

    assert(protonBinaryPath("/x/tuxblox") == "/x/tuxblox/ProtonBuild/dist/proton");

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

    printf("process_launcher: all tests passed\n");
    return 0;
}
