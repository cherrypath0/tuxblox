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
//
// Portions derived from Proton's proton.py:
// Copyright (c) 2018-2022, Valve Corporation. All rights reserved.
// Licensed under the 3-clause BSD license; see
// third_party_licenses/proton/LICENSE.proton for the full text.

#pragma once
#include <string>
#include <vector>

#include "proton.h"
#include "util.h"

namespace tuxblox {

// Which graphics backend the prefix should be set up for. Roblox usually
// renders through its own Vulkan backend, but it can fall back to Direct3D or
// OpenGL, so both paths have to keep working.
struct GraphicsConfig {
    // Use Wine's OpenGL-based wined3d instead of Vulkan-based DXVK.
    bool useWineD3D = false;
    // Let DXVK provide dxgi. Turned off when wined3d is in use, or when the
    // user overrode dxgi by hand.
    bool useDxvkDxgi = true;
    // Disable d3d11 entirely.
    bool noD3D11 = false;
    // Provide NVAPI to the Direct3D path on NVIDIA hardware.
    bool useNvapi = true;
};

// One launch: the environment Wine runs under, the process itself, and the
// wait for the prefix to empty afterwards.
class Session {
public:
    Session(Proton& proton, std::filesystem::path prefixDir);
    ~Session();

    // Fills in the library, DLL and data paths Wine needs to find its own
    // files. Runs before the prefix is set up.
    void initWine();

    // Applies the launch settings: logging, graphics, synchronization and the
    // DLL overrides the prefix setup asked for.
    void initSession();

    // Writes the collected DLL overrides into the environment. Called after
    // the prefix setup, which adds the overrides for whichever Direct3D
    // files it installed.
    void applyDllOverrides();

    // Runs a command to completion and returns its exit code. Ctrl+C or a
    // SIGTERM tears down the whole process tree and the prefix with it.
    int runProc(const std::vector<std::string>& command);
    int runProc(const std::vector<std::string>& command, const Environment& localEnv);

    // Launches the target and waits for the prefix to empty. Returns the
    // target's own exit code.
    int run(const std::vector<std::string>& target);

    // Waits for every process in the prefix to disconnect from wineserver.
    // The timeout only starts once no Roblox process is left, so a live
    // session is never cut short.
    void waitForPrefixDrain(int timeoutSeconds);

    Environment env;

    // Build version, recorded in the log header.
    std::string buildVersion;

    // DLL overrides collected before launch, written out as WINEDLLOVERRIDES.
    std::map<std::string, std::string> dllOverrides;

    // Resolved by initSession, consumed by the prefix setup to pick which
    // Direct3D implementation to install.
    GraphicsConfig graphics;

private:
    void openLogFile();
    void writeLogHeader(const std::vector<std::string>& target);

    // Re-emits the wrapped process's real exit code, which it writes to the
    // log rather than to this process's own stderr.
    void relayRealExitCode();

    Proton& proton;
    std::filesystem::path prefixDir;
    int logFd = -1;
    std::filesystem::path logPath;
};

// True when the prefix still holds a running Roblox process, as opposed to
// only leftover helpers.
bool prefixHasSessionHolder(const std::filesystem::path& prefixDir);

} // namespace tuxblox
