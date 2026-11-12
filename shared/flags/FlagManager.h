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
#include <functional>
#include <map>
#include <optional>
#include <string>

namespace tuxblox {

// Where a flag's value comes from, highest authority first: $TUXBLOX_ROOT/flags.json,
// then the server, then the values compiled in. Per key, not per source -- a key the
// override does not set falls to the server, and a key neither sets falls to the default.

// Fetches a URL and returns its body, or throws. Injected so the decision to ask, and
// what is done with the answer, can be tested without a server.
using FlagFetch = std::function<std::string(const std::string& url, int timeoutSeconds)>;

// This machine's architecture, as the flag path spells it.
const char* flagArch();

// The URL for a channel's flags. No release: there is one set per channel and
// architecture, so a flag does not revert when somebody updates.
std::string clientFlagsUrl(const std::string& channel, const std::string& arch);

// Loads the compiled-in defaults and starts the fetch. Returns at once; the FIRST flag
// read waits for the answer, up to `timeoutSeconds`, and seals the table -- so a flag
// cannot change value during a run, and nothing waits on the network except that read.
//
// `isRecordedRelease` false means no request at all. That is every local run out of
// build/, which neither waits nor appears in the server's logs.
//
// `fetch` empty means the real one. `baseUrl` empty means config.tuxblox.net; a caller
// that honours TUXBLOX_CONFIG_HOST passes the host it resolved, so the suffix guard
// lives with the caller that already has it rather than being copied here.
// `userAgent` empty means a plain product string. Passed in for the same reason as
// baseUrl: taking it from shared/release would drag release_file.cpp into the
// compatibility layer, which is a large dependency for one header line.
void initFlags(const std::string& installDir, const std::string& channel,
               bool isRecordedRelease, int timeoutSeconds,
               FlagFetch fetch = {}, const std::string& baseUrl = {},
               const std::string& userAgent = {});

// Forgets everything, so initFlags() can run again. A second initFlags() WITHOUT this is
// ignored: the table is sealed at the first read and a flag must not change value during
// a run, so re-initialising by accident would hand one feature two answers in one
// session. Calling this is a deliberate act and says so.
void resetFlags();

// A name no source defines answers the same as one defined and switched off. An offline
// install whose defaults predate a flag cannot tell those apart, so nor may a caller.
bool flagBool(const std::string& name);
int64_t flagInt(const std::string& name);
std::string flagString(const std::string& name);

// --- what the flags decide about a request ------------------------------

// Where a request is going, which is what decides which of the three HTTP-version flags
// applies to it.
enum class HttpDestination {
    RobloxCdn,   // *.rbxcdn.com, where Roblox's own files come from
    TuxbloxCdn,  // setup*.tuxblox.net, where TuxBlox's own files come from
    Other,       // everything else: the flag server, telemetry, the geo lookup
};

// Which of the three a URL belongs to. By host, so one call at each place a request is
// built covers every caller of it -- the same downloader fetches a TuxBlox component and
// a Roblox package, and only the URL tells them apart.
HttpDestination httpDestinationFor(const std::string& url);

// Asks libcurl for the HTTP version the flags name for that destination. A flag that says
// nothing leaves the handle alone, which is libcurl's own default and what every request
// did before these flags existed -- so an install that has never reached the server
// behaves exactly as it used to.
//
// `curl` is a CURL*, taken as void* so this header does not drag curl.h behind it.
void applyFlaggedHttpVersion(void* curl, const std::string& url);

// --- what a flag set is -------------------------------------------------
//
// Below this line is the shape of a set and the rules for reading one: the two sources
// above are parsed into these, and a test can check the rules without a server.

// TuxBlox's own feature flags: switches shipped off and turned on later, without a
// release. A flag does something only because compiled code asks for it by name, so a
// name nothing reads is inert -- the server can choose between paths that were built in,
// never create one.

enum class FlagType { Bool, String, Int };

struct FlagValue {
    FlagType type = FlagType::Bool;
    bool boolean = false;
    int64_t integer = 0;
    std::string text;
};

using FlagTable = std::map<std::string, FlagValue>;

// The type a name declares: tbxFlag is a boolean, tbxString a string, tbxInt an integer.
// Nothing for a name that declares none, which is how an unknown name is refused rather
// than guessed at.
std::optional<FlagType> flagTypeForName(const std::string& name);

// True for a flag the server may not set. tbxFlagDisableUpdater is the one that matters:
// served wrongly it stops every install updating, and updating is how that would be
// corrected. The list is compiled in and the client filters the server's answer against
// it, because the server is not trusted to respect its own rule.
bool isLocalOnlyFlag(const std::string& name);

// The flags in a JSON object, dropping any whose value contradicts its name, any name
// with no known prefix, and -- when `fromServer` -- any local-only name. Never throws:
// anything that is not a flat object of usable values reads as an empty table, which
// leaves the sources below it answering.
FlagTable parseFlagJson(const std::string& json, bool fromServer);

// Applies `higher` over `base`, per key. A key `higher` does not set is left alone, and a
// key only `higher` has is added -- which is how a flag is introduced ahead of the
// release that reads it.
void mergeFlagsOver(FlagTable& base, const FlagTable& higher);

} // namespace tuxblox
