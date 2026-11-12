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

#include "i18n.h"
#include "release_file.h"

#include <filesystem>

namespace tuxblox {

AboutVersions aboutVersions(const std::string& installDir, const std::string& launcherVersion) {
    AboutVersions about;

    const RecordedRelease recorded = readRecordedRelease(installDir);
    if (recorded.present) {
        about.release = recorded.file.release;
        about.channel = recorded.file.channel;
    }

    for (const std::string& name : componentNames()) {
        // Left out on purpose: the interface stack is GTK and libadwaita built from source, so its
        // version tracks theirs and says nothing a reader of this screen can act on. It is still a
        // component, still downloaded and still named while it installs -- just not listed here.
        if (name == "libtuxblox") continue;
        const std::string display = componentLayout(name)->displayName;
        // The launcher is the one asking, so it answers from the binary that is running rather
        // than from a copy on disk, which may not be the one that was started.
        if (name == "launcher") {
            about.components.push_back({display, launcherVersion});
            continue;
        }
        // What the install records, not what the binary answers: a binary names the release it was
        // published in now, which is one number for all of them and not what this list is for. The
        // record is not the weaker source it would once have been -- an install whose components do
        // not match it is one the launcher is already forcing a repair on.
        const std::string path = componentLayout(name)->versionPath;
        const auto recordedComponent = recorded.file.components.find(name);
        if (path.empty() || !std::filesystem::exists(installDir + "/" + path)) {
            about.components.push_back({display, i18n::tr("not installed")});
        } else if (recordedComponent != recorded.file.components.end()) {
            about.components.push_back({display, recordedComponent->second.version});
        } else {
            // On disk under an install that records nothing, so the only thing left to ask is the
            // component itself, and what it answers is all there is to show.
            about.components.push_back(
                {display, readComponentIdentity(installDir, name).value_or(i18n::tr("not installed"))});
        }
    }
    return about;
}

} // namespace tuxblox
