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

#include "support/filelock.h"

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace tuxblox {

FileLock::FileLock(std::filesystem::path lockPath) : path(std::move(lockPath)) {}

FileLock::~FileLock() {
    if (descriptor >= 0) {
        ::close(descriptor);
    }
}

void FileLock::acquire() {
    if (depth++ > 0) {
        return;
    }

    descriptor = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0666);
    if (descriptor < 0) {
        depth = 0;
        throw std::runtime_error("Could not open lock file \"" + path.string() +
                                 "\": " + std::strerror(errno));
    }

    int locked = 0;
    do {
        locked = ::flock(descriptor, LOCK_EX);
    } while (locked != 0 && errno == EINTR);

    if (locked != 0) {
        const int failure = errno;
        ::close(descriptor);
        descriptor = -1;
        depth = 0;
        throw std::runtime_error("Could not lock \"" + path.string() +
                                 "\": " + std::strerror(failure));
    }
}

void FileLock::release() {
    if (--depth > 0) {
        return;
    }
    if (descriptor >= 0) {
        ::flock(descriptor, LOCK_UN);
        ::close(descriptor);
        descriptor = -1;
    }
}

FileLock::Guard::Guard(FileLock& lock) : lock(lock) {
    lock.acquire();
}

FileLock::Guard::~Guard() {
    lock.release();
}

} // namespace tuxblox
