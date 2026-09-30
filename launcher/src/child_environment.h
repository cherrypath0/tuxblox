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
#include <vector>

#include "adw_env.h"

namespace tuxblox {

// The environment a started program gets: this process's, less what the interface libraries changed. Built before fork() so nothing allocates between fork and exec.
class ChildEnvironment {
public:
    ChildEnvironment() : entries_(unbundledEnvironment()) {
        for (std::string &entry : entries_) pointers_.push_back(entry.data());
        pointers_.push_back(nullptr);
    }
    ChildEnvironment(const ChildEnvironment &) = delete;
    ChildEnvironment &operator=(const ChildEnvironment &) = delete;

    char **envp() { return pointers_.data(); }

private:
    std::vector<std::string> entries_;
    std::vector<char *> pointers_;
};

} // namespace tuxblox
