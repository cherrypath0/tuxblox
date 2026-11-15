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

namespace tuxblox {

AboutVersions aboutVersions(const std::string& installDir, const std::string& launcherVersion,
                             const std::string& release, const std::string& channel) {
    AboutVersions about;

    // The caller's own stamp: every component of a release carries it, so there is nothing beside
    // them to read and nothing that can go stale against what is actually installed.
    about.release = release;
    about.channel = channel;

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
        // The component's own number, not the release it was published in: the release is one
        // number for all of them and is shown above, while this list is for what moved.
        about.components.push_back(
            {display, readComponentVersion(installDir, name).value_or(i18n::tr("not installed"))});
    }
    return about;
}

} // namespace tuxblox
