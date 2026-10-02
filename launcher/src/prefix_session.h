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
#include <string>
#include <vector>

namespace tuxblox {

// Which Roblox processes are live in a virtual drive. Clients are what a
// session is; an installer holds the drive without being one, so it is counted
// apart -- a first-ever install must not turn a Launch button into Stop.
struct PrefixSessions {
    int player = 0;
    int studio = 0;
    int installers = 0;
    std::vector<int> playerPids;
    std::vector<int> studioPids;
};

// Mirrors prefixSessionHolders() in compat/tuxblox/src/support/sessions.cpp and
// must stay in sync with it, including the image lists. The launcher needs its
// own copy because it has to answer this before deciding which run option to
// invoke, and because the two halves carry different licences.
PrefixSessions prefixSessions(const std::string& prefixDir);

// True if any Roblox process is currently live inside `prefixDir` -- the
// WINEPREFIX, i.e. installDir + "/runtime/pfx", NOT installDir + "/runtime".
//
// Mirrors prefixHasSessionHolder() in compat/tuxblox/src/launch/session.cpp and
// must stay in sync with it, including the image allowlist. The launcher needs
// its own copy because it has to answer this BEFORE deciding which Proton verb
// to invoke -- see ProcessLauncher::launch().
bool prefixHasSessionHolder(const std::string& prefixDir);

// Sends SIGTERM to every process running inside `prefixDir`, waits briefly,
// then SIGKILLs whatever ignored it -- Wine apps under Proton routinely
// ignore SIGTERM, the same reason ProcessLauncher::stop() escalates.
// Returns how many processes were signalled. The calling process is never
// included, so the launcher cannot terminate itself.
int terminatePrefixProcesses(const std::string& prefixDir);

// Testable seams, exposed for tests rather than for callers.

// Every pid under `procRoot` whose WINEPREFIX is `prefixDir`, excluding the
// caller. Unlike prefixHasSessionHolderIn() this ignores the image
// allowlist: terminating a prefix has to take wineserver and the Wine
// services with it, not just the Roblox processes. An empty `prefixDir`
// matches nothing rather than everything.
std::vector<int> collectPrefixPidsIn(const std::string& procRoot, const std::string& prefixDir);

// Extracts the lowercased image basename from a wine process's
// /proc/<pid>/cmdline first token, or "" if it isn't a wine process.
std::string wineImageNameFromCmdline(const std::string& firstCmdlineToken);

// prefixHasSessionHolder() against an arbitrary /proc-shaped root.
bool prefixHasSessionHolderIn(const std::string& procRoot, const std::string& prefixDir);

// prefixSessions() against an arbitrary /proc-shaped root.
PrefixSessions prefixSessionsIn(const std::string& procRoot, const std::string& prefixDir);

// The value of `key` in a NUL-delimited /proc/<pid>/environ blob, or "" if it
// has no such entry. A key that is only a prefix of another never matches it.
std::string envValueFromEnviron(const std::string& environBlob, const std::string& key);

} // namespace tuxblox
