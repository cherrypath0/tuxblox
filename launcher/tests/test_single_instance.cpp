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

#include "single_instance.h"
#include <cassert>
#include <cstdio>
#include <csignal>
#include <filesystem>
#include <sys/wait.h>
#include <unistd.h>

namespace fs = std::filesystem;

// Reproduces the "watcher process keeps the launcher locked out" bug.
//
// The GUI launcher takes the lock, then hands off to a detached
// --watch-launch helper via fork+fork+execl (App::requestLaunch) and exits.
// flock() locks belong to the open file description, and a plain open()'d fd
// survives execl(), so without O_CLOEXEC the helper inherits the very
// description holding the lock and keeps it held for the whole Roblox
// session -- every later launcher start then reports "already running".
//
// Modelled exactly on that shape: a child takes the lock, exec's a
// long-sleeping grandchild, and exits. Once the child is gone, nothing that
// is still a *launcher* holds the lock, so acquiring it must succeed.
static void testLockNotInheritedByExecdChild() {
    using namespace tuxblox;

    fs::path dir = fs::temp_directory_path() / "tuxblox_test_single_instance_exec";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // The helper touches this once it has actually reached exec. The
    // inherited fd is only closed *at* exec, so "the watcher is active" --
    // the state the user hits, and the state this test is about -- means
    // post-exec. Waiting for it keeps the test deterministic instead of
    // racing the helper's own fork->exec window, which the real launcher
    // never observes anyway (the GUI still holds the lock itself until its
    // own shutdown finishes, long after the helper has exec'd).
    fs::path ready = dir / "helper_ready";

    int pipefd[2];
    assert(pipe(pipefd) == 0);

    pid_t child = fork();
    assert(child >= 0);
    if (child == 0) {
        close(pipefd[0]);
        // Stand in for the GUI launcher: take the lock, then exec a helper
        // and exit, leaving the helper running detached.
        if (!acquireSingleInstanceLock(dir.string())) _exit(2);
        pid_t grandchild = fork();
        if (grandchild == 0) {
            std::string cmd = "touch '" + ready.string() + "'; sleep 30";
            execl("/bin/sh", "sh", "-c", cmd.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        ssize_t n = write(pipefd[1], &grandchild, sizeof(grandchild));
        _exit(n == static_cast<ssize_t>(sizeof(grandchild)) ? 0 : 3);
    }

    close(pipefd[1]);
    pid_t grandchild = -1;
    assert(read(pipefd[0], &grandchild, sizeof(grandchild)) == sizeof(grandchild));
    close(pipefd[0]);

    int status = 0;
    assert(waitpid(child, &status, 0) == child);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);

    for (int i = 0; i < 500 && !fs::exists(ready); ++i) usleep(10000);
    assert(fs::exists(ready));

    // The lock holder is gone; only the running helper is still alive. It
    // must not be keeping the lock held.
    bool acquired = acquireSingleInstanceLock(dir.string());

    if (grandchild > 0) kill(grandchild, SIGKILL);
    fs::remove_all(dir);

    assert(acquired == true);
}

int main() {
    using namespace tuxblox;

    // flock() locks belong to the open file description, not the process,
    // so acquiring it twice in this same test process still exercises real
    // mutual exclusion -- the second call opens a distinct file description
    // on the same lock file and must fail to lock it, exactly like a second
    // launcher process would.
    fs::path dir = fs::temp_directory_path() / "tuxblox_test_single_instance";
    fs::remove_all(dir);
    fs::create_directories(dir);

    assert(acquireSingleInstanceLock(dir.string()) == true);
    assert(acquireSingleInstanceLock(dir.string()) == false);

    fs::remove_all(dir);

    // installDir doesn't exist and can't be created (no such parent) --
    // fails open rather than blocking every launch on a lock file it can't
    // manage.
    assert(acquireSingleInstanceLock("/tuxblox_test_single_instance_no_such_parent/sub") == true);

    testLockNotInheritedByExecdChild();

    printf("single_instance: all tests passed\n");
    return 0;
}
