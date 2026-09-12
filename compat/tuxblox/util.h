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

#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include <sys/types.h>

namespace tuxblox {

// The environment passed to Wine, kept sorted so the order never depends on
// insertion order.
using Environment = std::map<std::string, std::string>;

// Options for copyPath. The defaults match the common case: copy a file,
// follow symlinks, and make sure the result stays writable.
struct CopyOptions {
    std::string prefix;
    bool addWritePerm = true;
    bool copyMetadata = false;
    bool optional = false;
    bool followSymlinks = true;
    bool linkDebug = false;
    std::ofstream *pTrackFile = nullptr;
};

// Writes a prefixed line to standard error. Never throws.
void log(const std::string& message);

// Joins arguments into one line for the log, quoting the ones that contain
// spaces so a path with a space still reads as a single argument.
std::string joinCommandLine(const std::vector<std::string>& arguments);

// Unlike std::filesystem::exists, this reports broken symlinks as present
// when followSymlinks is false.
bool fileExists(const std::filesystem::path& path, bool followSymlinks);

bool isSymlink(const std::filesystem::path& path);

void killProcessGroup(pid_t pgid, int signalNumber);

// True when a value is set and is not the string "0".
bool nonzero(const std::string& value);

void prependToEnvStr(Environment& env, const std::string& variable,
                     const std::string& value, const std::string& separator);
void appendToEnvStr(Environment& env, const std::string& variable,
                    const std::string& value, const std::string& separator);

// Detects the placeholder files Wine writes in place of real Windows DLLs, so
// the prefix upgrade can replace them without touching a user's own DLLs.
bool fileIsWineBuiltinDll(const std::filesystem::path& path);

// Creates a directory and its parents. A broken symlink in the way is removed
// first, which a plain create_directories would fail on.
void makeDirs(const std::filesystem::path& path);

// Copies files from src into dst without overwriting anything that already
// exists. Directories that already exist in dst are skipped entirely, because
// merging two copies of a save directory tends to corrupt it.
void mergeUserDir(const std::filesystem::path& src, const std::filesystem::path& dst);

// Copies a single file, applying the given options. Missing sources and
// permission errors are reported rather than thrown when optional is set.
void copyPath(const std::filesystem::path& src, const std::filesystem::path& dst,
              const CopyOptions& options = {});

// Copies file contents, using a reflink when the filesystem supports one.
void copyFile(const std::filesystem::path& src, const std::filesystem::path& dst);

// copyFile with the destination-is-a-directory and permission-error handling
// that the prefix setup relies on.
void tryCopyFile(const std::filesystem::path& src, const std::filesystem::path& dst);

// The file's modification time as a string, or "0" if it cannot be read.
// Used to tell whether a tracked file changed between launches.
std::string getMtimeStr(const std::filesystem::path& path);

// Asks ext4 to treat the directory as case-insensitive. Silently does nothing
// on filesystems that do not support it.
void setDirCasefoldBit(const std::filesystem::path& path);

} // namespace tuxblox
