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
    // Where the release says these bytes are, relative to client/. Asked first, so the layout can
    // change without stranding anyone: a client that reads this follows the release wherever it
    // goes, where one that builds the address itself can only ever find the layout it shipped
    // knowing. Empty when the release does not say, or says something that is not a relative path
    // under client/, and then the address is built as before.
    std::string path;
};

struct ReleaseComponent {
    std::string version;  // "x.y.z", this component's own, carrying no channel
    std::optional<ComponentBytes> x86_64;
    std::optional<ComponentBytes> arm64;
};

struct ReleaseFile {
    std::string release;   // "MAJOR.MINOR.BUILD.PATCH" -- the counter is the third field
    std::string channel;
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
    std::string displayName;   // shown while it installs, e.g. "Runtime"
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

// Orders two four-field release versions: negative when `a` is behind `b`, 0 when they are
// the same, positive when it is ahead. Field by field and numerically, so 3.10.0.0 is ahead of
// 3.9.0.0 -- which text order gets backwards, and which is the whole reason this exists.
//
// A version with a field that is not a number, or without four of them, sorts BELOW every
// well-formed one rather than throwing: this decides which release an install takes, and a
// malformed answer from the server must not be able to stop it updating.
int compareReleases(const std::string& a, const std::string& b);

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

// Where a release's copy of a component is published. A release publishes its own copy of every
// component, because each copy carries the release it belongs to inside it -- so one component
// version has a file per release it appears in, and the release is part of the address. No channel:
// a release number is the same release wherever it is published, and only the release URLs below
// name a channel.
std::string componentUrl(const std::string& baseUrl, const std::string& name,
                          const std::string& version, const std::string& release,
                          const std::string& arch);

// Where a component sat when one copy of it served every release it appeared in. Tried only after
// componentUrl() has failed, which is what keeps the last release published under the old layout
// installable from a client that already knows the new one.
std::string legacyComponentUrl(const std::string& baseUrl, const std::string& name,
                                const std::string& version, const std::string& arch);

// Where the release itself says a component's bytes are. Asked before either address above, so a
// client follows the layout the release describes rather than the one it was built knowing. Empty
// when the release said nothing usable, and then the addresses above are all there is.
std::string publishedUrl(const std::string& baseUrl, const std::string& path);

// Every address worth trying for a component, best first: where the release says its bytes are,
// then the address this build would construct, then where one copy served every release that named
// it. Each tier is offered on every base before the next tier is, because a base that cannot answer
// is a different failure from an address that was never published: the edge is an optimisation and
// the origin behind it is the truth. Empty when the release does not publish this component here.
std::vector<std::string> componentUrls(const std::vector<std::string>& baseUrls,
                                        const ReleaseFile& release, const std::string& name,
                                        const std::string& arch);

// The bases a download should try, in order: the one chosen, then the origin behind it when that
// one was the edge. Every caller of componentUrls() wants this, and a caller that passes its base
// alone turns an edge outage into a download that cannot happen rather than one that is slower.
std::vector<std::string> componentBaseUrls(const std::string& baseUrl);
std::string latestPointerUrl(const std::string& baseUrl, const std::string& channel);
std::string releaseFileUrl(const std::string& baseUrl, const std::string& channel,
                            const std::string& release);

// --- what an install says it is -----------------------------------------

// An install says what it is through its own components: every one of them carries the release it
// was published in, so there is nothing beside them to read and nothing to go stale.

// What the interface stack answers. It is libraries rather than a program, so it carries version.so
// purely to be asked, and that library exports the release it was published in, the stack's own
// version, and the channel that release went to. std::nullopt when the stack is absent, or carries
// no such library, which is an install made before it did.
struct StackVersions {
    std::string release;    // empty for a stack that was never published
    std::string component;  // the stack's own version, which is the one thing it always knows
    std::string channel;    // empty for a stack that was never published
};
std::optional<StackVersions> readStackVersions(const std::string& installDir);

// What the copy of `name` on disk says it is, or std::nullopt when this install holds none. One
// rule, shared, because the launcher deciding "this install is coherent" and the installer deciding
// "this one is already current" must not be able to reach different answers about the same install.
//
// Every component answers with the RELEASE it was published in, since that is what a binary is
// asked for its version now, and what the interface stack's version.so exports.
//
// A component that is present but cannot be asked -- not executable, exits nonzero, prints nothing
// -- answers std::nullopt, the same as one that is absent. There is nothing to compare, and a
// placeholder would differ from every real answer forever.
std::optional<std::string> readComponentIdentity(const std::string& installDir,
                                                  const std::string& name);

// The component's OWN version, which is what moved when it moved: 3.7.2 for a launcher, whatever
// shared/ui/versions.env declares for the interface stack. Every program answers this one with
// --component-version, and the stack's version.so exports it. std::nullopt on the same terms as
// readComponentIdentity(): absent, or present and unable to answer.
std::optional<std::string> readComponentVersion(const std::string& installDir,
                                                 const std::string& name);

// Negative, zero or positive as `left` sorts below, with, or above `right`. Dotted numbers,
// field by field, a missing field counting as zero, so "3.11" and "3.11.0" are the same number.
// Anything that is not a number compares equal: an unreadable version is not evidence either way.
int compareDottedVersions(const std::string& left, const std::string& right);

} // namespace tuxblox
