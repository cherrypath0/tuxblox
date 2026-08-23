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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#include "util.h"

#include <array>
#include <cerrno>
#include <cstring>
#include <string_view>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <linux/fs.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

const std::string LogPrefix = "Proton: ";

// Directories Wine has installed its builtin DLLs into, across the versions of
// Proton this prefix may have been created by. A symlink pointing into any of
// them is ours to replace, even when the target is gone.
const std::array<std::string_view, 12> WineBuiltinDllDirs = {
    "/lib/wine/i386-unix",
    "/lib/wine/i386-windows",
    "/lib/wine/x86_64-unix",
    "/lib/wine/x86_64-windows",
    "/lib/wine/aarch64-unix",
    "/lib/wine/aarch64-windows",
    "/lib/wine",
    "/lib/wine/fakedlls",
    "/lib64/wine",
    "/lib64/wine/fakedlls",
    "/lib64/wine/x86_64-unix",
    "/lib64/wine/x86_64-windows"
};

const std::array<std::string_view, 2> WineBuiltinDllTags = {
    "Wine placeholder DLL",
    "Wine builtin DLL"
};

bool endsWith(std::string_view text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool isPermissionDenied(const std::error_code& error) {
    return error.value() == EPERM;
}

bool isMissing(const std::error_code& error) {
    return error.value() == ENOENT;
}

// One copy_file_range call, retried past interruptions. Returns the number of
// bytes copied, or -1 with errno set.
ssize_t copyFileRange(int inputFd, int outputFd, size_t count) {
    ssize_t copied = 0;
    do {
        copied = static_cast<ssize_t>(
            syscall(SYS_copy_file_range, inputFd, nullptr, outputFd, nullptr, count, 0u));
    } while (copied < 0 && errno == EINTR);
    return copied;
}

// Falls back to a plain read/write loop when the kernel or the filesystem
// cannot reflink, which is what copy_file_range reports through these codes.
bool reflinkUnsupported(int errorNumber) {
    return errorNumber == EXDEV || errorNumber == ENOSYS ||
           errorNumber == EINVAL || errorNumber == EOPNOTSUPP;
}

} // namespace

void log(const std::string& message) {
    const std::string line = LogPrefix + message + "\n";
    // Deliberately ignores the result: if stderr is gone there is nothing
    // useful a second write could report.
    ssize_t written = ::write(STDERR_FILENO, line.data(), line.size());
    static_cast<void>(written);
}

bool fileExists(const fs::path& path, bool followSymlinks) {
    std::error_code error;
    const fs::file_status status =
        followSymlinks ? fs::status(path, error) : fs::symlink_status(path, error);
    return fs::exists(status);
}

bool isSymlink(const fs::path& path) {
    std::error_code error;
    return fs::is_symlink(fs::symlink_status(path, error));
}

void killProcessGroup(pid_t pgid, int signalNumber) {
    if (::killpg(pgid, signalNumber) != 0 && errno != ESRCH) {
        log("Could not signal process group " + std::to_string(pgid) + ": " +
            std::strerror(errno));
    }
}

bool nonzero(const std::string& value) {
    return !value.empty() && value != "0";
}

void prependToEnvStr(Environment& env, const std::string& variable,
                     const std::string& value, const std::string& separator) {
    auto existing = env.find(variable);
    if (existing == env.end()) {
        env[variable] = value;
    } else {
        existing->second = value + separator + existing->second;
    }
}

void appendToEnvStr(Environment& env, const std::string& variable,
                    const std::string& value, const std::string& separator) {
    auto existing = env.find(variable);
    if (existing == env.end()) {
        env[variable] = value;
    } else {
        existing->second = existing->second + separator + value;
    }
}

bool fileIsWineBuiltinDll(const fs::path& path) {
    if (isSymlink(path)) {
        std::error_code error;
        const fs::path target = fs::read_symlink(path, error);
        if (!error) {
            const std::string targetDir = target.parent_path().string();
            for (const std::string_view& dllDir : WineBuiltinDllDirs) {
                if (endsWith(targetDir, dllDir)) {
                    return true;
                }
            }
        }
    }

    if (!fileExists(path, true)) {
        return false;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Wine writes its marker just past the DOS header.
    file.seekg(0x40);
    std::array<char, 20> tag = {};
    file.read(tag.data(), tag.size());
    const std::string_view found(tag.data(), static_cast<size_t>(file.gcount()));

    for (const std::string_view& builtinTag : WineBuiltinDllTags) {
        if (found.size() >= builtinTag.size() &&
            found.compare(0, builtinTag.size(), builtinTag) == 0) {
            return true;
        }
    }
    return false;
}

void makeDirs(const fs::path& path) {
    std::error_code error;
    if (isSymlink(path) && !fileExists(path, true)) {
        fs::remove(path, error);
    }
    fs::create_directories(path, error);
}

void mergeUserDir(const fs::path& src, const fs::path& dst) {
    std::error_code error;
    std::vector<fs::path> extantDirs;

    fs::recursive_directory_iterator walk(
        src, fs::directory_options::skip_permission_denied, error);
    if (error) {
        return;
    }

    // The source root itself is not produced by the iterator, so handle it and
    // then every directory underneath it the same way.
    std::vector<fs::path> sourceDirs = {src};
    for (const fs::directory_entry& entry : walk) {
        if (entry.is_directory(error) && !entry.is_symlink(error)) {
            sourceDirs.push_back(entry.path());
        }
    }

    for (const fs::path& sourceDir : sourceDirs) {
        const fs::path relative = sourceDir.lexically_relative(src);
        if (relative.empty()) {
            continue;
        }
        const fs::path destinationDir =
            (relative == ".") ? dst : dst / relative;

        bool childOfExtantDir = false;
        for (const fs::path& extant : extantDirs) {
            const fs::path fromExtant = fs::relative(destinationDir, extant, error);
            if (!error && !fromExtant.empty() &&
                fromExtant.native().compare(0, 2, "..") != 0) {
                childOfExtantDir = true;
                break;
            }
        }
        if (childOfExtantDir) {
            continue;
        }

        // Only copy into directories that do not exist yet. The destination
        // root is the exception, since it is created before the merge starts.
        const bool isDestinationRoot =
            fileExists(destinationDir, true) && fs::equivalent(destinationDir, dst, error);
        if (fileExists(destinationDir, true) && !isDestinationRoot) {
            extantDirs.push_back(destinationDir);
            continue;
        }

        makeDirs(destinationDir);

        fs::directory_iterator children(sourceDir, error);
        if (error) {
            continue;
        }
        for (const fs::directory_entry& child : children) {
            const fs::path destinationChild = destinationDir / child.path().filename();
            if (fileExists(destinationChild, true)) {
                continue;
            }
            // Directories are recreated by the walk above; only their symlinks
            // need copying here, alongside every file.
            const bool childIsDir = child.is_directory(error) && !child.is_symlink(error);
            if (childIsDir) {
                continue;
            }
            CopyOptions options;
            options.copyMetadata = true;
            options.followSymlinks = false;
            copyPath(child.path(), destinationChild, options);
        }
    }
}

void copyPath(const fs::path& src, const fs::path& dst, const CopyOptions& options) {
    std::error_code error;

    fs::path destination = dst;
    if (!options.prefix.empty()) {
        destination = fs::path(options.prefix) / destination;
    }
    if (fs::is_directory(destination, error)) {
        destination /= src.filename();
    }

    if (fileExists(destination, false)) {
        fs::remove(destination, error);
    } else if (options.pTrackFile != nullptr && !options.prefix.empty()) {
        *options.pTrackFile << destination.lexically_relative(options.prefix).string()
                            << "\n";
    }

    if (isSymlink(src) && !options.followSymlinks) {
        fs::copy_symlink(src, destination, error);
        if (error) {
            if (options.optional && isMissing(error)) {
                log("Error while copying to \"" + destination.string() +
                    "\": " + error.message());
                return;
            }
            if (!isPermissionDenied(error)) {
                throw fs::filesystem_error("copy_symlink failed", src, destination, error);
            }
            log("Error while copying to \"" + destination.string() + "\": " +
                error.message());
            return;
        }
    } else {
        try {
            copyFile(src, destination);
        } catch (const fs::filesystem_error& failure) {
            if (options.optional && isMissing(failure.code())) {
                log("Error while copying to \"" + destination.string() +
                    "\": " + failure.code().message());
                return;
            }
            // Permission problems are tolerated: if one really matters, the
            // launch fails later with a clearer error than this one.
            if (isPermissionDenied(failure.code())) {
                log("Error while copying to \"" + destination.string() + "\": " +
                    failure.code().message());
                return;
            }
            throw;
        }
    }

    // Linux has no way to change a symlink's own mode or timestamps, so a
    // symlink copied as a symlink keeps whatever copy_symlink produced.
    const bool destinationIsSymlink = isSymlink(destination);
    if (!destinationIsSymlink) {
        if (options.copyMetadata) {
            const fs::file_time_type modified = fs::last_write_time(src, error);
            if (!error) {
                fs::last_write_time(destination, modified, error);
            }
        }

        const fs::perms sourcePerms = options.followSymlinks
            ? fs::status(src, error).permissions()
            : fs::symlink_status(src, error).permissions();
        if (!error && sourcePerms != fs::perms::unknown) {
            fs::permissions(destination, sourcePerms, fs::perm_options::replace, error);
        }

        if (options.addWritePerm) {
            fs::permissions(destination, fs::perms::owner_write | fs::perms::group_write,
                            fs::perm_options::add, error);
        }
    }

    // Separate debug symbols, when the source ships them, are linked rather
    // than copied so the dist keeps a single copy.
    const fs::path srcDebug = src.string() + ".debug";
    const fs::path destinationDebug = destination.string() + ".debug";
    bool linkDebug = options.linkDebug && fileExists(srcDebug, true);

    if (fileExists(destinationDebug, false)) {
        fs::remove(destinationDebug, error);
    } else if (linkDebug && options.pTrackFile != nullptr && !options.prefix.empty()) {
        *options.pTrackFile
            << destinationDebug.lexically_relative(options.prefix).string() << "\n";
    }

    if (linkDebug) {
        fs::create_symlink(srcDebug, destinationDebug, error);
    }
}

void copyFile(const fs::path& src, const fs::path& dst) {
    const int inputFd = ::open(src.c_str(), O_RDONLY | O_CLOEXEC);
    if (inputFd < 0) {
        throw fs::filesystem_error("could not open source", src,
                                   std::error_code(errno, std::generic_category()));
    }

    struct stat sourceInfo = {};
    if (::fstat(inputFd, &sourceInfo) != 0) {
        const int failure = errno;
        ::close(inputFd);
        throw fs::filesystem_error("could not stat source", src,
                                   std::error_code(failure, std::generic_category()));
    }

    const int outputFd =
        ::open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (outputFd < 0) {
        const int failure = errno;
        ::close(inputFd);
        throw fs::filesystem_error("could not open destination", dst,
                                   std::error_code(failure, std::generic_category()));
    }

    off_t remaining = sourceInfo.st_size;
    bool reflinked = true;
    while (remaining > 0) {
        const ssize_t copied =
            copyFileRange(inputFd, outputFd, static_cast<size_t>(remaining));
        if (copied < 0) {
            if (!reflinkUnsupported(errno)) {
                const int failure = errno;
                ::close(inputFd);
                ::close(outputFd);
                throw fs::filesystem_error("could not copy", src, dst,
                                           std::error_code(failure, std::generic_category()));
            }
            reflinked = false;
            break;
        }
        if (copied == 0) {
            break;
        }
        remaining -= copied;
    }

    ::close(inputFd);
    ::close(outputFd);

    if (!reflinked) {
        std::error_code error;
        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, error);
        if (error) {
            throw fs::filesystem_error("could not copy", src, dst, error);
        }
    }
}

void tryCopyFile(const fs::path& src, const fs::path& dst) {
    std::error_code error;
    fs::path destination = dst;
    if (fs::is_directory(destination, error)) {
        destination /= src.filename();
    }
    if (fileExists(destination, false)) {
        fs::remove(destination, error);
    }
    try {
        copyFile(src, destination);
    } catch (const fs::filesystem_error& failure) {
        if (!isPermissionDenied(failure.code())) {
            throw;
        }
        log("Error while copying to \"" + destination.string() + "\": " +
            failure.code().message());
    }
}

std::string getMtimeStr(const fs::path& path) {
    struct stat info = {};
    if (::stat(path.c_str(), &info) != 0) {
        return "0";
    }
    return std::to_string(info.st_mtime);
}

void setDirCasefoldBit(const fs::path& path) {
    const int directoryFd = ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directoryFd < 0) {
        return;
    }

    unsigned int attributes = 0;
    if (::ioctl(directoryFd, FS_IOC_GETFLAGS, &attributes) >= 0) {
        attributes |= FS_CASEFOLD_FL;
        ::ioctl(directoryFd, FS_IOC_SETFLAGS, &attributes);
    }
    ::close(directoryFd);
}

} // namespace tuxblox
