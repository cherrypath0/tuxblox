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

#include <algorithm>

namespace tuxblox {

namespace {

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

// Component order rather than the map's, because this ends up in a line somebody reads and
// compares against another run.
std::vector<std::string> inComponentOrder(const std::map<std::string, std::string>& recorded) {
    std::vector<std::string> ordered;
    for (const std::string& name : componentNames()) {
        if (recorded.count(name)) ordered.push_back(name);
    }
    // A release cannot name anything outside the five -- parseReleaseFile refuses it -- but this
    // state is built by callers, and silently dropping a component from the coherence check is the
    // one failure mode that would never show up as a wrong answer, only as a missing one.
    for (const auto& [name, version] : recorded) {
        (void)version;
        if (std::find(ordered.begin(), ordered.end(), name) == ordered.end()) ordered.push_back(name);
    }
    return ordered;
}

} // namespace

UpdateDecision decideUpdate(const InstallState& state, const std::string& namedRelease,
                             bool noRollback) {
    UpdateDecision decision;

    // Nothing published on this channel yet. Not an error, and not something to repair towards.
    if (namedRelease.empty()) return decision;

    if (!state.releaseRecorded) {
        // There is nothing to check coherence against, so the only question left is whether
        // something is published -- and it is. Offered rather than forced: the install works, it
        // just cannot say what it is, and that is not a reason to replace it without asking.
        decision.update = true;
        decision.reason = "this install does not record which release it is";
        return decision;
    }

    for (const std::string& name : inComponentOrder(state.recordedVersions)) {
        const auto installed = state.installedVersions.find(name);
        if (installed == state.installedVersions.end() ||
            installed->second != state.recordedVersions.at(name)) {
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
                               " match release " + state.recordedRelease + ", but norollback is set";
            // Deciding the repair first means an ordinary update is not reached either, so say so
            // rather than leaving someone who set this by hand wondering why a release never comes.
            if (state.recordedRelease != namedRelease) {
                decision.reason += " -- release " + namedRelease + " is published and is not being taken";
            }
            return decision;
        }
        decision.update = true;
        decision.forced = true;
        decision.reason = "the " + listed(decision.incoherent) + agrees +
                           " match release " + state.recordedRelease + ", so this install is being repaired";
        return decision;
    }

    // No direction to this: a channel pointed back at an earlier release is taking that release
    // back, and the installs that already have it are the ones that have to follow.
    if (state.recordedRelease != namedRelease) {
        decision.update = true;
        decision.reason = "this install is release " + state.recordedRelease + " and " +
                           namedRelease + " is published";
    }
    return decision;
}

} // namespace tuxblox
