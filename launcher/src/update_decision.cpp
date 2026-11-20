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

#include "update_decision.h"

#include "release_file.h"
#include "version.h"


namespace tuxblox {

namespace {

// The installer is a tool fetched to apply an update rather than part of the running install, and
// ensureInstallerBinary() checks it against the release before running it, so its release says
// nothing about whether this install is whole. A Flatpak carries no interface libraries either.
bool carriedHere(const std::string& name) {
    if (name == "installer") return false;
    if constexpr (kFlatpakBuild) return name != "libtuxblox";
    return true;
}

// By the names TuxBlox uses everywhere else, not by the keys the release file happens to use:
// this ends up on a terminal and in bug reports.
std::string listed(const std::vector<std::string>& names) {
    std::string out;
    for (size_t i = 0; i < names.size(); i++) {
        if (i > 0) out += i + 1 == names.size() ? " and " : ", ";
        const std::optional<ComponentLayout> layout = componentLayout(names[i]);
        out += layout.has_value() ? layout->displayName : names[i];
    }
    return out;
}

} // namespace

std::vector<std::string> channelsAbove(const std::string& channel) {
    // Written out rather than derived from an index into the ladder, because the ladder is
    // three names and the question only ever has three answers.
    if (channel == "experimental") return {"canary", "stable"};
    if (channel == "canary") return {"stable"};
    return {};
}

UpdateDecision decideUpdate(const InstallState& state, const std::string& namedRelease,
                             bool noRollback) {
    UpdateDecision decision;

    // Nothing published on this channel yet. Not an error, and not something to repair towards.
    if (namedRelease.empty()) return decision;

    // Ahead of the channel in the release or in any single component: built here, not downloaded.
    // Repairing it would fetch the published release over the top of the build, and offering it an
    // older release is not an update, so nothing is done at all.
    if (compareDottedVersions(state.installedRelease, namedRelease) > 0) {
        decision.internal = true;
        decision.reason = "this install is ahead of " + namedRelease + ", so it is left alone";
        return decision;
    }
    for (const auto& [name, version] : state.installedVersions) {
        const auto published = state.publishedVersions.find(name);
        if (published == state.publishedVersions.end()) continue;
        if (compareDottedVersions(version, published->second) > 0) {
            decision.internal = true;
            decision.reason = listed({name}) + " " + version + " is ahead of the published " +
                              published->second + ", so this install is left alone";
            return decision;
        }
    }

    if (state.installedRelease.empty()) {
        // There is nothing to check coherence against, so the only question left is whether
        // something is published -- and it is. Offered rather than forced: the install works, it
        // just cannot say what it is, and that is not a reason to replace it without asking.
        decision.update = true;
        decision.reason = "this install cannot say which release it is";
        return decision;
    }

    // Every component of a release carries that release inside it, so an install is coherent when
    // all of them name the same one. A component that is absent, or that cannot be asked, is as
    // much a half-applied update as one naming a different release.
    for (const std::string& name : componentNames()) {
        if (!carriedHere(name)) continue;
        const auto installed = state.installedIdentities.find(name);
        if (installed == state.installedIdentities.end() ||
            installed->second != state.installedRelease) {
            decision.incoherent.push_back(name);
        }
    }

    if (!decision.incoherent.empty()) {
        const std::string agrees = decision.incoherent.size() == 1 ? " does not" : " do not";
        if (noRollback) {
            // An install built on this machine has its components replaced one at a time, so it is
            // incoherent by construction, and "repairing" it means fetching the published release
            // over the top of the local build. The cost is that a genuinely half-applied update is
            // left alone too, which is why this is off by default and has no switch in the window.
            decision.reason = "the " + listed(decision.incoherent) + agrees +
                               " match release " + state.installedRelease + ", but norollback is set";
            // Deciding the repair first means an ordinary update is not reached either, so say so
            // rather than leaving someone who set this by hand wondering why a release never comes.
            if (state.installedRelease != namedRelease) {
                decision.reason += " -- release " + namedRelease + " is published and is not being taken";
            }
            return decision;
        }
        decision.update = true;
        decision.forced = true;
        decision.reason = "the " + listed(decision.incoherent) + agrees +
                           " match release " + state.installedRelease + ", so this install is being repaired";
        return decision;
    }

    // No direction to this: a channel pointed back at an earlier release is taking that release
    // back, and the installs that already have it are the ones that have to follow.
    if (state.installedRelease != namedRelease) {
        decision.update = true;
        decision.reason = "this install is release " + state.installedRelease + " and " +
                           namedRelease + " is published";
    }
    return decision;
}

} // namespace tuxblox
