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
#include <cstdint>
#include <string>

namespace tuxblox {

// The folder to install into: TUXBLOX_ROOT when it names an absolute path,
// else this binary's own directory when that already holds an install (the
// persisted installer is re-run from there to apply an update), else
// $HOME/.tuxblox. Throws std::runtime_error if it comes to HOME and HOME is
// not set.
std::string installDir();

// One spelling for a folder path, so "/home/me", "/home/me/" and "/home/me/."
// compare equal. Every caller-supplied path goes through this before it is
// compared against anything or has its parent taken -- a trailing separator
// otherwise survives normalisation and defeats both.
std::string normalizedDir(const std::string& dir);

// The nearest folder at or above `dir` that exists, so a filesystem check can
// be made against a folder the install has not created yet. Ends at "/".
std::string existingAncestor(const std::string& dir);

// True when this directory holds a TuxBlox install rather than being some
// unrelated folder the installer happens to be sitting in.
bool looksLikeInstall(const std::string& dir);

// The running binary's real path, via /proc/self/exe rather than argv[0],
// which can be relative or a bare basename depending on how it was started.
std::string selfExePath();

// True if the filesystem containing `path` has at least `minBytes` free.
// `path` must already exist (its containing filesystem is checked).
bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes);

} // namespace tuxblox
