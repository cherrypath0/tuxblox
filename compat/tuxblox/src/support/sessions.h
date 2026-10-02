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
#include <string>
#include <vector>

namespace tuxblox {

// A running Roblox process that keeps the virtual drive's session alive, as
// opposed to a leftover helper.
struct SessionHolder {
    std::string pid;
    std::string image;
    // The Player or Studio client itself, rather than one of the installers.
    bool client = false;
};

// The bare file name of a path, for matching against the image lists.
std::string imageNameOf(const std::string& path);

bool imageIsClient(const std::string& image);

bool imageIsInstaller(const std::string& image);

// Every Roblox process still running in the virtual drive. Empty means only
// helpers are left and the session is over.
// Mirrored by prefixSessions() in launcher/src/prefix_session.cpp, which must
// stay in sync with the image lists here -- the launcher cannot share this file
// because the two halves carry different licences.
std::vector<SessionHolder> prefixSessionHolders(const std::filesystem::path& prefixDir);

// The sessions a launch of `image` has to replace. Roblox runs one Player
// session at a time and closes the running one when a new Player starts, so a
// new Player launch stands the running one down first; a second Player that
// finds the first still there closes itself within 50 ms instead of starting.
// Empty for Studio, which runs as many sessions as are asked for, and for the
// installers, which are mid-handover and must be left alone.
std::vector<std::string> sessionsToReplace(const std::string& image,
                                           const std::vector<SessionHolder>& holders);

// What a session closing must do with the virtual drive.
enum class DriveExit {
    // A session that joined somebody else's drive: never its to close.
    Leave,
    // The last one out closes the drive behind it.
    TearDownNow,
    // This session owns the drive but others are still using it, so it has to
    // stay until the last of them leaves and close the drive then. Leaving now
    // would leave the drive running with nobody left who will ever close it.
    WaitThenTearDown
};

DriveExit driveExitOnClose(bool ownsPrefix, const std::vector<SessionHolder>& remaining);

// The other compatibility-layer processes running `image` for this virtual
// drive. Matched on the resolved exe rather than the command line, so another
// install's processes are left alone whatever they were invoked as.
std::vector<std::string> otherLayerProcesses(const std::filesystem::path& procRoot,
                                             const std::filesystem::path& layerBinary,
                                             const std::string& tuxbloxPrefix,
                                             const std::string& image, int selfPid);

// Closes the session already running in this virtual drive so a new one can
// take its place, and waits for it to actually go -- a new Player that starts
// while the old one is still there closes itself instead.
void replaceRunningSession(const std::filesystem::path& prefixDir, const std::string& image,
                           const std::filesystem::path& layerBinary,
                           const std::string& tuxbloxPrefix);

// Testable seams, exposed for tests rather than for callers.

// prefixSessionHolders() against an arbitrary /proc-shaped root.
std::vector<SessionHolder> prefixSessionHoldersIn(const std::filesystem::path& procRoot,
                                                 const std::filesystem::path& prefixDir);

} // namespace tuxblox
