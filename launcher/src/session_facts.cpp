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

#include "session_facts.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace tuxblox {

namespace {

const unsigned long long kMebibyte = 1024ULL * 1024ULL;
const unsigned long long kGibibyte = kMebibyte * 1024ULL;

std::string readWholeLineFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) return "";
    std::ostringstream all;
    all << file.rdbuf();
    return all.str();
}

} // namespace

long bootTimeSeconds(const std::string& procRoot) {
    std::ifstream file(procRoot + "/stat");
    if (!file) return 0;
    std::string line;
    while (std::getline(file, line)) {
        if (line.rfind("btime ", 0) != 0) continue;
        return std::atol(line.c_str() + 6);
    }
    return 0;
}

std::time_t processStartTime(const std::string& procRoot, int pid) {
    const long boot = bootTimeSeconds(procRoot);
    if (boot <= 0) return 0;

    const std::string stat = readWholeLineFile(procRoot + "/" + std::to_string(pid) + "/stat");
    // The executable's own name is the second field and is free to contain spaces and brackets, so the fields are counted from the last closing bracket rather than from the start of the line.
    const size_t afterName = stat.rfind(')');
    if (afterName == std::string::npos) return 0;

    std::istringstream fields(stat.substr(afterName + 1));
    std::string field;
    // The first field after the name is the third; the start time is the twenty-second.
    for (int index = 3; index <= 22; ++index) {
        if (!(fields >> field)) return 0;
    }

    const long ticksPerSecond = sysconf(_SC_CLK_TCK);
    if (ticksPerSecond <= 0) return 0;
    return static_cast<std::time_t>(boot + std::atoll(field.c_str()) / ticksPerSecond);
}

unsigned long long processResidentBytes(const std::string& procRoot, int pid) {
    std::ifstream file(procRoot + "/" + std::to_string(pid) + "/statm");
    if (!file) return 0;
    unsigned long long total = 0;
    unsigned long long resident = 0;
    if (!(file >> total >> resident)) return 0;
    const long pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize <= 0) return 0;
    return resident * static_cast<unsigned long long>(pageSize);
}

std::string formatUptime(long seconds) {
    if (seconds < 0) seconds = 0;
    char text[32];
    if (seconds < 60) {
        std::snprintf(text, sizeof(text), "%lds", seconds);
    } else if (seconds < 3600) {
        std::snprintf(text, sizeof(text), "%ldm", seconds / 60);
    } else if (seconds < 86400) {
        std::snprintf(text, sizeof(text), "%ldh %02ldm", seconds / 3600, (seconds % 3600) / 60);
    } else {
        std::snprintf(text, sizeof(text), "%ldd %02ldh", seconds / 86400, (seconds % 86400) / 3600);
    }
    return text;
}

std::string formatBytes(unsigned long long bytes) {
    char text[32];
    if (bytes >= kGibibyte) {
        std::snprintf(text, sizeof(text), "%.2f GB", static_cast<double>(bytes) / kGibibyte);
    } else {
        // Rounded rather than truncated, so a figure a hair under the next whole number does not read as a hair over the previous one.
        std::snprintf(text, sizeof(text), "%llu MB", (bytes + kMebibyte / 2) / kMebibyte);
    }
    return text;
}

} // namespace tuxblox
