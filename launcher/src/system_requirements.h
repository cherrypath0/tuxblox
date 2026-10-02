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

// One requirement this machine does not meet, in the words the user is shown.
struct UnmetRequirement {
    std::string requirement; // "Linux 6.7 or newer"
    std::string found;       // "6.1.0-generic", or empty when nothing could be read
};

// What the checks read. Separate from the machine so a test can describe one without being one.
struct SystemFacts {
    std::string kernelRelease;      // uname()'s release, e.g. "6.14.2-arch1-1"
    std::string nvidiaProcContent;  // /proc/driver/nvidia/version, empty when there is no NVIDIA driver
    bool vulkanLoadable = true;     // libvulkan.so.1 could be opened
};

// Whether `release` names a kernel older than major.minor.
//
// A release that cannot be read is NOT older. Every check here fails open for the same reason: being
// unable to tell is not evidence that a requirement is unmet, and refusing to start on a guess locks
// someone out of software they have already installed.
bool kernelIsOlderThan(const std::string& release, int major, int minor);

// The driver version out of /proc/driver/nvidia/version, e.g. "550.107.02". Empty when the content is
// not that file. Both the proprietary and the open module are read: they word the line differently and
// are held to the same floor.
std::string nvidiaDriverVersion(const std::string& procContent);

// Whether `version` is older than `minimum`, comparing dotted numbers part by part so 1000 is not read
// as older than 999. Unparseable either side is not older -- see kernelIsOlderThan.
bool versionIsOlder(const std::string& version, const std::string& minimum);

// Every published requirement `facts` does not meet. Empty means TuxBlox will run.
//
// Only what can be proved is here. Mesa's version is not readable without glxinfo installed, and the
// amount of memory or disk a machine has says nothing certain about whether Roblox will run on it, so
// neither is ever a reason to refuse.
std::vector<UnmetRequirement> unmetRequirements(const SystemFacts& facts);

// This machine. Never throws: anything unreadable is left empty, which no check treats as a failure.
SystemFacts collectSystemFacts();

// What to show the user, or empty when nothing is unmet.
std::string unsupportedSystemMessage(const std::vector<UnmetRequirement>& unmet);

} // namespace tuxblox
