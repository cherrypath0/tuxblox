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

namespace tuxblox {

// Keeps a session log from growing without end. With detailed logging on, the
// log grows by megabytes a second, and a report arrived of one reaching 26 GB --
// a file nobody can open, let alone send. Watching a log file is the only place
// this can be solved: everything writing to it is Roblox or the compatibility
// layer, neither of which can be asked to say less without losing the lines
// that explain a crash.
//
// Trimming takes the middle out and keeps both ends, because each end answers a
// different question: the start says what was launched and how, and the end says
// what went wrong. A limit that kept only one of them would leave half the
// reports unanswerable.

// Starts watching the log behind `fd`. Ignores anything that is not an ordinary
// file, so a launch from a terminal is untouched. Safe to call for more than one
// log; a fd already watched is not added twice.
void watchLogFile(int fd);

// Stops watching a log, before its descriptor is closed. Descriptor numbers are
// reused the moment they are free, so an entry left behind would be matched
// against whatever file opens next.
void unwatchLogFile(int fd);

// Trims every watched log that has grown past the limit. Cheap enough to call on
// a poll loop -- it is one fstat() per log until one actually needs trimming.
void trimWatchedLogs();

} // namespace tuxblox
