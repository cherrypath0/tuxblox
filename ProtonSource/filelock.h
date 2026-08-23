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

#pragma once
#include <filesystem>

namespace tuxblox {

// An exclusive lock on a file, used to keep two launches from setting up the
// same prefix at once. Acquiring waits for as long as it takes.
//
// Hold one by constructing a FileLock::Guard; the lock is released when the
// guard goes out of scope, including when an exception unwinds past it.
class FileLock {
public:
    explicit FileLock(std::filesystem::path lockPath);
    ~FileLock();

    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    class Guard {
    public:
        explicit Guard(FileLock& lock);
        ~Guard();

        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;

    private:
        FileLock& lock;
    };

private:
    void acquire();
    void release();

    std::filesystem::path path;
    int descriptor = -1;
    int depth = 0;
};

} // namespace tuxblox
