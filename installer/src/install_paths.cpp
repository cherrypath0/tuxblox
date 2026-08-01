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

#include "install_paths.h"
#include <cstdlib>
#include <stdexcept>
#include <sys/statvfs.h>

namespace tuxblox {

std::string installDir() {
    const char* home = std::getenv("HOME");
    if (!home || home[0] == '\0') {
        throw std::runtime_error("installDir: HOME environment variable is not set");
    }
    return std::string(home) + "/.local/share/tuxblox";
}

bool hasEnoughDiskSpace(const std::string& path, uint64_t minBytes) {
    struct statvfs st{};
    if (statvfs(path.c_str(), &st) != 0) {
        throw std::runtime_error("hasEnoughDiskSpace: statvfs failed for " + path);
    }
    uint64_t freeBytes = static_cast<uint64_t>(st.f_bavail) * static_cast<uint64_t>(st.f_frsize);
    return freeBytes >= minBytes;
}

} // namespace tuxblox
