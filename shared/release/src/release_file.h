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
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

// A release is a set of independently versioned components, and this is the one place that knows
// what one looks like. The launcher reads a release to decide whether an install is current, the
// installer reads the same bytes to install it, and the install keeps a copy to say what it is --
// so the three can never disagree about what a release consists of.
namespace tuxblox {

// One architecture's published file. Absent, never a placeholder: `size: 0` already means "unknown,
// fall back to the Content-Length" to the download progress, so a zeroed entry would fail somewhere
// downstream instead of failing immediately with something that can be explained.
struct ComponentBytes {
    std::string sha256;
    uint64_t size = 0;
};

struct ReleaseComponent {
    std::string version;  // "x.y.z", this component's own, carrying no channel
    std::optional<ComponentBytes> x86_64;
    std::optional<ComponentBytes> arm64;
};

struct ReleaseFile {
    std::string release;   // "MAJOR.MINOR.PATCH.BUILD"
    std::string channel;
    // "components" when each component version carries its own changelog, "release" for a backfilled
    // historical release that has one document for the whole thing.
    std::string changelog;
    std::string sourceCommit;
    bool sourceDirty = false;
    std::map<std::string, ReleaseComponent> components;
};

// Where a component lands in an install and how it is described while it is landing. The release
// file carries none of this -- it describes what a release IS, not what an install looks like.
struct ComponentLayout {
    std::string publishedAs;   // its name on the CDN, extension included
    std::string filename;      // its name in the install; an archive extracts to a folder of this name
    std::string path;          // relative to the install root, "/" for the top level
    std::string displayName;   // shown while it installs, e.g. "Compatibility layer"
    // What to ask for its version, relative to the install root. Not always `filename`: the layer
    // installs as a folder and the program inside it is what answers. Empty for the one component
    // that has no binary at all.
    std::string versionPath;
    bool isArchive = false;
};

// Throws std::runtime_error with a sentence naming what is wrong and which component it is wrong in.
//
// A component this build does not know is CARRIED, not refused. Releases gain components; a client
// that rejects the whole release file on meeting one it has never heard of would stop updating the
// day that happens, permanently and with no way to reach it. Callers skip what componentLayout()
// does not recognise -- an old client then still takes the update, and the launcher and installer
// it receives know the rest.
ReleaseFile parseReleaseFile(const std::string& jsonText);

// The five components a release consists of, in a fixed order. There is no bootstrapper: it was
// folded into the launcher in 3.4.0.
const std::vector<std::string>& componentNames();

// std::nullopt for a name that is not one of ours, rather than a guess at where it would go.
std::optional<ComponentLayout> componentLayout(const std::string& name);

// --- where downloads come from -------------------------------------------

// A syntactically valid name under tuxblox.net: the name itself, or any subdomain of it. Case is
// ignored. Anything carrying a scheme, a port, credentials, a path, a trailing dot or an empty
// label is not a host and is refused.
bool isTuxbloxHost(const std::string& host);

// True if the name resolves. Only ever asked about an override, never about the default, so an
// ordinary launch makes no DNS query it would not otherwise make.
bool hostResolves(const std::string& host);

// "https://" + host when `host` is a tuxblox.net name, and the real CDN otherwise. Syntax only --
// the resolve check lives in releasesBaseUrl(), so this stays pure and testable. The scheme is
// never the caller's to choose: an override that could say http:// would turn a redirect into a
// downgrade.
std::string releasesBaseUrlFor(const char* host);

// The edge TuxBlox uses by default. A base equal to this one falls back to the origin when it
// cannot answer; a base somebody asked for explicitly does NOT, because an explicit redirect means
// "use this, I mean it" -- quietly reaching production instead would make a test against a local
// server pass for the wrong reason.
const std::string& releasesEdgeUrl();

// The origin, always. Bunny sits in front of this, so when the edge cannot answer, this still can.
const std::string& releasesOriginUrl();

// Where this process downloads TuxBlox from, decided once.
//
// By default the edge (setup-bunny.tuxblox.net), which caches component bytes close to whoever is
// downloading them while still asking the origin for the pointer that decides whether an update
// exists. releasesOriginUrl() is what every fetch falls back to when the edge cannot answer, so the
// edge being down delays an update rather than making one impossible.
//
// $TUXBLOX_RELEASES_CDN_HOST redirects it, for trying a CDN layout against a real server before it
// reaches the name every installed TuxBlox asks. It falls back to the origin when the name is not a
// tuxblox.net one and when it does not resolve -- a bad value in the environment must never stop
// TuxBlox updating itself from the right place.
//
// Anything able to set this can usually also replace the binary it redirects, so the allow-list is
// what carries the weight: an attacker who can set it still cannot point it anywhere they control,
// short of owning a tuxblox.net subdomain. It says so on stderr whenever it is honoured, so it can
// never quietly redirect somebody's updates.
const std::string& releasesBaseUrl();

// The architecture this build runs on, and the one its downloads come from. Only x86_64 is built
// today; arm64 is reserved in the layout, which is why a release names both and publishes one.
const char* hostArch();

// A component version's bytes are shared by every channel, so its URL carries no channel. A release
// belongs to one channel, so both of the release URLs do.
std::string componentUrl(const std::string& baseUrl, const std::string& name,
                          const std::string& version, const std::string& arch);
std::string latestPointerUrl(const std::string& baseUrl, const std::string& channel);
std::string releaseFileUrl(const std::string& baseUrl, const std::string& channel,
                            const std::string& release);

// --- what an install records about itself -------------------------------

std::string recordedReleasePath(const std::string& installDir);

struct RecordedRelease {
    bool present = false;  // false for an install made before this file existed, which is ordinary
    ReleaseFile file;      // valid iff present
};

// Never throws. A missing, unreadable, truncated or wrongly-shaped file all read the same way --
// "this install does not say which release it is" -- because that is an ordinary state an install
// can be in, and the launcher reads this on the path that decides whether it can start at all.
RecordedRelease readRecordedRelease(const std::string& installDir);

// Writes the release file's PUBLISHED bytes, not a re-serialisation of them, so what an install
// records is exactly what the server served. Written last by the installer, which makes it the
// "update completed" marker as well as the install's identity.
bool writeRecordedRelease(const std::string& installDir, const std::string& publishedJson);

// The interface stack has no binary to ask, so it carries its version as a file inside its tarball.
std::optional<std::string> readStackVersion(const std::string& installDir);

// What version of `name` this install holds, or std::nullopt when it holds none. One rule, shared,
// because the launcher deciding "this install is coherent" and the installer deciding "this one is
// already current" must not be able to reach different answers about the same install.
//
// A component that is present but cannot be asked -- not executable, exits nonzero, prints nothing
// -- answers std::nullopt, the same as one that is absent. There is no version to compare, and a
// placeholder would differ from every real version forever.
std::optional<std::string> readComponentVersion(const std::string& installDir,
                                                 const std::string& name);

} // namespace tuxblox
