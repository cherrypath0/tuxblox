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
    int fd = open(path.c_str(), O_CREAT | O_RDWR, 0644);
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
