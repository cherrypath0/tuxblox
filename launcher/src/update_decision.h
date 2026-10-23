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
#include <map>
#include <string>
#include <vector>

// Whether an install should be replaced, and whether the user gets a say.
//
// Components carry their own versions now, so five different numbers in one install is the normal
// state rather than evidence of a half-applied update. That turns the old single question -- "do
// the pieces agree on one build id" -- into two that are answered separately:
//
//   Coherent?  Do the installed versions match the component set of the release the install
//              RECORDS? Incoherent means an update died midway, which is broken rather than out of
//              date, so it is repaired without asking.
//   Stale?     Does the recorded release differ from the one the channel names? An ordinary
//              update, which waits for the Auto-Update setting. It covers a channel moving BACK to
//              an earlier release too, because "recorded != named" has no direction.
//
// Getting this wrong fails both ways -- broken installs that stop repairing themselves, or every
// install repairing itself on every launch forever -- so the decision is pure and the filesystem
// and the network stay on the other side of it.
namespace tuxblox {

struct InstallState {
    // False for an install made before the release file existed. Not an error: it just cannot be
    // asked whether it is coherent, only whether something newer is published.
    bool releaseRecorded = false;
    std::string recordedRelease;
    // Component name -> the version it reports. A component that is not installed is absent from
    // the map rather than present with an empty string.
    std::map<std::string, std::string> installedVersions;
    // Component name -> the version the recorded release names. Empty when !releaseRecorded.
    std::map<std::string, std::string> recordedVersions;
};

struct UpdateDecision {
    bool update = false;
    // True iff the update bypasses the Auto-Update setting. An install whose pieces do not match
    // the release it claims to be is not a state the user chose.
    bool forced = false;
    // The components that disagree with the recorded release, in component order. Filled even when
    // noRollback suppressed the update, so the caller can say what it saw and did not act on.
    std::vector<std::string> incoherent;
    std::string reason;  // one sentence for the log; empty when nothing is to be done
};

// `namedRelease` is what the channel's `latest` names, or empty when the channel publishes nothing.
// `noRollback` means "never take the forced path" -- it protects an install built on this machine,
// whose components are replaced one at a time and which is therefore incoherent by construction.
UpdateDecision decideUpdate(const InstallState& state, const std::string& namedRelease,
                             bool noRollback);

} // namespace tuxblox
