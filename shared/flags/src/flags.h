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
void initFlags(const std::string& installDir, const std::string& channel,
               bool isRecordedRelease, int timeoutSeconds,
               FlagFetch fetch = {}, const std::string& baseUrl = {});

// A name no source defines answers the same as one defined and switched off. An offline
// install whose defaults predate a flag cannot tell those apart, so nor may a caller.
bool flagBool(const std::string& name);
int64_t flagInt(const std::string& name);
std::string flagString(const std::string& name);

} // namespace tuxblox
