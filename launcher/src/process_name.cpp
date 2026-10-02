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

#include "process_name.h"
#include <sys/prctl.h>

namespace tuxblox {

namespace {

// Linux stores the name in 16 bytes including the terminator.
constexpr size_t kMaxProcessNameLength = 15;

} // namespace

bool setProcessName(const std::string& name) {
    if (name.empty()) return false;
    // Truncated here rather than left to the kernel, which does it silently -- this way what was asked
    // for and what the process is actually listed as are the same string.
    const std::string trimmed = name.substr(0, kMaxProcessNameLength);
    return prctl(PR_SET_NAME, trimmed.c_str(), 0, 0, 0) == 0;
}

std::string currentProcessName() {
    char buffer[kMaxProcessNameLength + 2] = {0};
    if (prctl(PR_GET_NAME, buffer, 0, 0, 0) != 0) return "";
    buffer[kMaxProcessNameLength] = '\0';
    return buffer;
}

} // namespace tuxblox
