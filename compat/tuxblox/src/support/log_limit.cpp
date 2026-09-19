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

#include "support/log_limit.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace tuxblox {
namespace {

// Megabytes. TUXBLOX_LOG_LIMIT overrides it, and 0 turns trimming off for
// someone who genuinely wants the whole stream on disk.
constexpr long long DefaultLimitMB = 64;
constexpr long long BytesPerMB = 1024 * 1024;

struct WatchedLog {
    int fd = -1;
    // A log is opened write-only, and pread() on a write-only descriptor fails
    // with EBADF -- so keeping the end of the log needs a second, readable
    // descriptor for the same file, reopened through /proc/self/fd. Without one
    // the log can still be kept to a size, just not with its end intact.
    int readFd = -1;
    bool appends = false;
    long long trims = 0;
};

std::vector<WatchedLog>& watched() {
    static std::vector<WatchedLog> logs;
    return logs;
}

long long limitBytes() {
    static const long long bytes = [] {
        const char *setting = std::getenv("TUXBLOX_LOG_LIMIT");
        long long megabytes = DefaultLimitMB;
        if (setting != nullptr && *setting != '\0') {
            char *end = nullptr;
            const long long parsed = std::strtoll(setting, &end, 10);
            // A value that is not a number at all leaves the default in place
            // rather than silently turning the limit off.
            if (end != setting && parsed >= 0) {
                megabytes = parsed;
            }
        }
        return megabytes * BytesPerMB;
    }();
    return bytes;
}

// The head is what was launched and how; the tail is what went wrong. Trimming
// at the limit down to these two leaves room to grow again before the next one,
// so a long session trims a handful of times rather than on every poll.
long long headBytes() { return limitBytes() / 4; }
long long tailBytes() { return limitBytes() / 4; }

bool writeAllAt(int fd, const char *data, size_t length, off_t offset) {
    while (length > 0) {
        const ssize_t done = ::pwrite(fd, data, length, offset);
        if (done <= 0) {
            if (done < 0 && errno == EINTR) continue;
            return false;
        }
        data += done;
        length -= static_cast<size_t>(done);
        offset += done;
    }
    return true;
}

bool readAllAt(int fd, char *data, size_t length, off_t offset) {
    while (length > 0) {
        const ssize_t got = ::pread(fd, data, length, offset);
        if (got <= 0) {
            if (got < 0 && errno == EINTR) continue;
            return false;
        }
        data += got;
        length -= static_cast<size_t>(got);
        offset += got;
    }
    return true;
}

void trimOne(WatchedLog& log) {
    struct stat info {};
    if (::fstat(log.fd, &info) != 0 || !S_ISREG(info.st_mode)) return;
    if (info.st_size <= limitBytes()) return;

    const long long head = headBytes();
    const long long tail = log.readFd >= 0 ? tailBytes() : 0;
    const long long dropped = info.st_size - head - tail;

    std::vector<char> keep(static_cast<size_t>(tail));
    if (tail > 0 && !readAllAt(log.readFd, keep.data(), keep.size(), info.st_size - tail)) return;

    log.trims++;
    char notice[256];
    const int noticeLength = std::snprintf(
        notice, sizeof(notice),
        "\n====== TuxBlox removed %lld MB from the %s of this log (trim %lld). "
        "Set TUXBLOX_LOG_LIMIT to change the size, or 0 to keep everything. ======\n",
        dropped / BytesPerMB, tail > 0 ? "middle" : "rest", log.trims);
    if (noticeLength <= 0) return;

    // Shorten first and write the kept end back afterwards, so the file on disk
    // is a valid log at every instant -- a reader opening it mid-trim sees a
    // short log, never a corrupt one.
    if (::ftruncate(log.fd, head) != 0) return;
    if (!writeAllAt(log.fd, notice, static_cast<size_t>(noticeLength), head)) return;
    if (!writeAllAt(log.fd, keep.data(), keep.size(), head + noticeLength)) return;

    // Roblox, Wine and this process all write through one shared file offset
    // (the launcher opened the log once and handed the same one to everybody),
    // so without moving that offset back the next write lands where the log used
    // to end and the file grows straight back to the size just reclaimed. A log
    // opened for appending needs none of this -- every write already goes to
    // whatever the end is at the time.
    //
    // A write landing in the moment between the shortening above and the seek
    // below goes to where the log used to end, leaving a hole and a size that
    // jumps straight back up. Nothing can close that window from one side of a
    // shared descriptor, and it costs nothing worth paying to avoid: the hole
    // occupies no disk, and the next trim a second later removes it.
    if (!log.appends) {
        ::lseek(log.fd, head + noticeLength + static_cast<off_t>(keep.size()), SEEK_SET);
    }
}

} // namespace

void watchLogFile(int fd) {
    if (fd < 0 || limitBytes() <= 0) return;

    struct stat info {};
    // A terminal, a pipe or /dev/null cannot be trimmed and must not be seeked,
    // so only an ordinary file is ever watched.
    if (::fstat(fd, &info) != 0 || !S_ISREG(info.st_mode)) return;

    // Replace rather than skip. A descriptor number is reused as soon as the
    // one before it is closed, so an entry matching this number can easily be a
    // closed log rather than this one -- keeping it would trim the wrong file,
    // or read the end of a file this log has nothing to do with.
    unwatchLogFile(fd);

    WatchedLog log;
    log.fd = fd;
    const int flags = ::fcntl(fd, F_GETFL);
    log.appends = flags >= 0 && (flags & O_APPEND) != 0;

    char self[64];
    std::snprintf(self, sizeof(self), "/proc/self/fd/%d", fd);
    log.readFd = ::open(self, O_RDONLY | O_CLOEXEC);

    watched().push_back(log);
}

void unwatchLogFile(int fd) {
    std::vector<WatchedLog>& logs = watched();
    for (size_t index = 0; index < logs.size(); index++) {
        if (logs[index].fd != fd) continue;
        if (logs[index].readFd >= 0) ::close(logs[index].readFd);
        logs.erase(logs.begin() + static_cast<long>(index));
        return;
    }
}

void trimWatchedLogs() {
    for (WatchedLog& log : watched()) {
        trimOne(log);
    }
}

} // namespace tuxblox
