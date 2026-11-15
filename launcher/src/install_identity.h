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

namespace tuxblox {

// Which number names this install, decided once for every screen that shows one.
//
// The release is it, and staging writes it into a fixed field in every binary, so this one answers
// for itself rather than from the record beside it. The channel cannot be compiled in the same way:
// one copy of each component serves all three channels, so a channel baked in at build time is
// simply whichever channel happened to be building -- a canary install would be told it is on
// stable. It stays the install's record to say.
struct InstallIdentity {
    // False for a local build with no release stamped into it and no release recorded beside it,
    // which is ordinary. The version is then this program's own, and a screen says so rather than
    // printing a number that reads like a release and is not one.
    bool isRelease = false;
    std::string version;
    // Empty whenever isRelease is false, and also for a stamped binary whose install records a
    // different release -- a screen then names the release without a channel instead of guessing.
    std::string channel;
};

// `release` and `channel` come from the install's own record; an empty release means it kept none.
InstallIdentity installIdentity(const std::string& release, const std::string& channel,
                                 const std::string& launcherVersion);

} // namespace tuxblox
