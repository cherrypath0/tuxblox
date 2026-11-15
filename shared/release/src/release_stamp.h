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
#include <string>

// The release a binary belongs to, carried inside the binary itself.
//
// A release version is not known while the build runs: it is derived when the build is staged, from
// what the channels already publish. So every TuxBlox program reserves a fixed-size field for it and
// the staging step writes the number into the finished file. That is what lets any of them say which
// release it is part of without reading a file beside it, and it is why a release publishes every
// component rather than reusing the ones whose own version did not move -- the field makes each
// release's copy of a component a different file.
namespace tuxblox {

// Empty for a binary that was never staged into a release, which is every local build.
const std::string& releaseVersion();

// The channel that release was published on, and empty for a binary that was never staged: a build
// cannot know its channel, because which one a release goes to is decided when it is staged. So
// this doubles as the answer to "was this published at all", which is what a local build out of the
// build folder must answer no to -- it has a release baked in from the VERSION file, but no channel.
const std::string& releaseChannel();

// The release, or `fallback` when there is none. What a program prints when it is asked its
// version: a local build has no release to name, so it names what it does have instead.
std::string reportedVersion(const std::string& fallback);

} // namespace tuxblox
