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
#include <ctime>
#include <string>

namespace tuxblox {

// When the machine last started, as a unix time. Zero when it cannot be read, which makes every age derived from it zero rather than wrong.
long bootTimeSeconds(const std::string& procRoot);

// When this process started, as a unix time, or zero if it is gone.
std::time_t processStartTime(const std::string& procRoot, int pid);

// How much memory this process is actually holding, in bytes, or zero if it is gone.
unsigned long long processResidentBytes(const std::string& procRoot, int pid);

// How long something has been going, short enough to sit on one line beside other facts: "41s", "18m", "1h 04m", "2d 03h". Never empty.
std::string formatUptime(long seconds);

// A size somebody can read at a glance rather than a count of bytes: "812 MB", "1.41 GB". Never empty.
std::string formatBytes(unsigned long long bytes);

} // namespace tuxblox
