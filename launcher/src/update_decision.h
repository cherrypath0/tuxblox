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
//   Coherent?  Does every installed component say it belongs to the same release as the launcher
//              asking? Incoherent means an update died midway, which is broken rather than out of
//              date, so it is repaired without asking.
//   Stale?     Does the release they agree on differ from the one the channel names? An ordinary
//              update, which waits for the Auto-Update setting. It covers a channel moving BACK to
//              an earlier release too, because "installed != named" has no direction.
//
// Getting this wrong fails both ways -- broken installs that stop repairing themselves, or every
// install repairing itself on every launch forever -- so the decision is pure and the filesystem
// and the network stay on the other side of it.
namespace tuxblox {

struct InstallState {
    // The release this install says it is, which is what its own launcher answers. Empty when the
    // launcher cannot be asked: then there is nothing to check the rest against, and the only
    // question left is whether something newer is published.
    std::string installedRelease;
    // Each installed component's own version, and the version the channel publishes for it. An
    // install ahead of the channel in either the release or any one component was built here rather
    // than downloaded, and is left alone: see `internal` below.
    std::map<std::string, std::string> installedVersions;
    std::map<std::string, std::string> publishedVersions;
    // Component name -> the release the copy on disk says it was published in. A component that is
    // not installed, or cannot be asked, is absent from the map rather than present empty.
    std::map<std::string, std::string> installedIdentities;
};

struct UpdateDecision {
    bool update = false;
    // True iff the update bypasses the Auto-Update setting. An install whose pieces do not match
    // the release it claims to be is not a state the user chose.
    bool forced = false;
    // The components that disagree with the recorded release, in component order. Filled even when
    // noRollback suppressed the update, so the caller can say what it saw and did not act on.
    std::vector<std::string> incoherent;
    // True when this install is ahead of what its channel publishes, which nothing downloaded can
    // be. Then nothing is updated and nothing is repaired, and the window says so in its title.
    bool internal = false;
    std::string reason;  // one sentence for the log; empty when nothing is to be done
};

// `namedRelease` is what the channel's `latest` names, or empty when the channel publishes nothing.
// `noRollback` means "never take the forced path" -- it protects an install built on this machine,
// whose components are replaced one at a time and which is therefore incoherent by construction.
// The channels an install following `channel` may also take a release from, calmest first.
//
// Only ever calmer ones: an install on canary will take a stable build that is ahead of what
// canary has published, and will follow canary again the moment canary publishes something
// newer -- but it is never given an experimental build, because that is the opposite of what
// choosing canary asked for. Empty for stable, which has nothing above it, so a stable install
// makes no extra request at all.
//
// This is what tbxFlagEnableChannelToUpperChannelUpgradeOnBuildPromotion switches on; with it
// off the list is empty for every channel and an install follows only its own.
std::vector<std::string> channelsAbove(const std::string& channel);

UpdateDecision decideUpdate(const InstallState& state, const std::string& namedRelease,
                             bool noRollback);

} // namespace tuxblox
