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
    // Which session started it, from the stamp every process a session launches inherits. Empty for
    // one that carries no stamp, which is nobody's.
    std::string sessionId;
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

// The name of the stamp, and the holders carrying one session's. Roblox relaunches itself, so the
// process a session waited on is not the one still running, and several sessions share the drive:
// the stamp is the only thing that says which Roblox belongs to which session. An unstamped holder
// is never anybody's, so it is treated like a session that joined the drive without setting it up.
extern const char* const SessionIdVariable;

// What this session stamps its processes with. The process number, because that already identifies
// a session; here rather than at either use so the one printed is the one inherited.
std::string currentSessionId();

std::vector<SessionHolder> holdersOfSession(const std::vector<SessionHolder>& holders,
                                            const std::string& sessionId);

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
    // Somebody else is still in the drive, so they will be the last one out and will close it.
    Leave,
    // The last one out closes the drive behind it.
    TearDownNow
};

// Decided by who is left rather than by who started the drive, so no session ever has to outlive
// its own Roblox just to close the drive for the others.
DriveExit driveExitOnClose(bool otherSessionsInDrive);

// True when another TuxBlox session is still running in this virtual drive, whichever Roblox
// program it launched. A Roblox that relaunched itself is not one: it has no session of its own, so
// leaving the drive to it would leave nobody who will ever close it.
bool otherSessionsInDrive(const std::filesystem::path& layerBinary, const std::string& tuxbloxPrefix);

// The drive is marked in use for as long as a session is in it, and the last one out clears the
// mark as it closes the drive. So a drive still marked in use with no session left was cut off
// rather than closed -- the machine lost power, or something killed it outright -- and whatever it
// was in the middle of writing was never finished.
void markDriveInUse(const std::filesystem::path& baseDir);
void markDriveClosed(const std::filesystem::path& baseDir);

// Whether the next launch has to put the drive back in order before it starts anything. A session
// running right now is what the mark is for, so it is not a fault.
bool driveNeedsRepair(const std::filesystem::path& baseDir, bool otherSessionsInDrive);

// The other compatibility-layer processes running `image` for this virtual
// drive, or every one of them whatever it launched when `image` is empty.
// Matched on the resolved exe rather than the command line, so another
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
