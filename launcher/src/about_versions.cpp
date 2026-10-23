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

#include "about_versions.h"

#include "release_file.h"

namespace tuxblox {

AboutVersions aboutVersions(const std::string& installDir, const std::string& launcherVersion) {
    AboutVersions about;

    const RecordedRelease recorded = readRecordedRelease(installDir);
    if (recorded.present) {
        about.release = recorded.file.release;
        about.channel = recorded.file.channel;
    }

    for (const std::string& name : componentNames()) {
        const std::string display = componentLayout(name)->displayName;
        // The launcher is the one asking, so it answers from the binary that is running rather
        // than from a copy on disk, which may not be the one that was started.
        if (name == "launcher") {
            about.components.push_back({display, launcherVersion});
            continue;
        }
        about.components.push_back(
            {display, readComponentVersion(installDir, name).value_or("not installed")});
    }
    return about;
}

} // namespace tuxblox
