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

#include "install_identity.h"

#include "release_stamp.h"

namespace tuxblox {

InstallIdentity installIdentity(const std::string& release, const std::string& channel,
                                 const std::string& launcherVersion) {
    // The stamp in this very binary, before anything beside it: it names the release this file was
    // published in, where the record is only what the install was last told and can be missing,
    // stale, or written by another install sharing the folder. The channel is still the record's to
    // say, and only while it names the same release -- one naming another release says nothing
    // about which channel this one came out on.
    const std::string& stamped = releaseVersion();
    if (!stamped.empty()) return {true, stamped, stamped == release ? channel : ""};

    if (release.empty()) return {false, launcherVersion, ""};
    // A release with no channel beside it would read as an install whose channel is unknown, which
    // is not a state a recorded release can be in -- a release belongs to exactly one channel.
    if (channel.empty()) return {false, launcherVersion, ""};
    return {true, release, channel};
}

} // namespace tuxblox
