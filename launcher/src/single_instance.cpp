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
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace tuxblox {

bool acquireSingleInstanceLock(const std::string& installDir) {
    std::string path = installDir + "/launcher.lock";
    // O_CLOEXEC matters: the GUI hands off to a detached --watch-launch
    // helper via fork+fork+execl (App::requestLaunch) and then exits. An fd
    // without it survives execl, and since flock() locks belong to the open
    // file description -- not the process -- the helper would inherit the
    // very description holding this lock and keep it held for the whole
    // Roblox session, long after the launcher itself is gone. Every launcher
    // start during a session then wrongly reported "already running".
    // O_CLOEXEC still survives fork(), so the lock covers the GUI process
    // itself for its whole life; it is dropped only at exec, which is
    // precisely the point where this process stops being a GUI launcher.
    int fd = open(path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0644);
    // Can't even manage the lock file itself (e.g. installDir doesn't exist
    // yet) -- fail open rather than blocking every launch on that.
    if (fd < 0) return true;

    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        close(fd);
        return false;
    }

    // Deliberately never closed: an flock() lock is released the moment its
    // file descriptor closes, so this fd has to stay open (leaked) for the
    // rest of the process's life for the lock to mean anything. That also
    // means it's released automatically on a crash, unlike a plain PID file.
    return true;
}

} // namespace tuxblox
