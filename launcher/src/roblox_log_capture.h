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
#include <utility>
#include <vector>

namespace tuxblox {

// Pure selection logic, kept separate from the real directory walk for
// testability: given (filename, mtime) pairs, returns just the names with
// mtime >= sessionStart, preserving input order.
std::vector<std::string> selectSessionLogFiles(
    const std::vector<std::pair<std::string, std::time_t>>& entries, std::time_t sessionStart);

std::string robloxLogsDir(const std::string& installDir);

std::vector<std::string> appendRobloxSessionLogs(const std::string& installDir, std::time_t sessionStart,
                                                  const std::string& destLogPath);

} // namespace tuxblox
