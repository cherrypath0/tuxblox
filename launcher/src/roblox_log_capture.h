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
#include <vector>

namespace tuxblox {

// The logs this session wrote, picked out of a folder that also holds other sessions' logs, earlier
// runs' logs and files that are not logs at all. A name is the only reliable evidence: every
// modification time in the folder moves while any session is running.
std::vector<std::string> selectSessionLogFiles(const std::vector<std::string>& names,
                                               std::time_t sessionStart, bool player);

std::string robloxLogsDir(const std::string& installDir);

std::vector<std::string> appendRobloxSessionLogs(const std::string& installDir, std::time_t sessionStart,
                                                  const std::string& destLogPath, bool player);

// Removes the logs appendRobloxSessionLogs() copied, so a session leaves one file holding both halves instead of two holding the same lines. Every name is checked against this target once more before it goes, because losing somebody else's log is far worse than leaving a copy behind.
int removeAppendedRobloxLogs(const std::string& installDir, const std::vector<std::string>& appended,
                             bool player);

} // namespace tuxblox
