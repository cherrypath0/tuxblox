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

#include "system_requirements.h"
#include <dlfcn.h>
#include <fstream>
#include <sstream>
#include <sys/utsname.h>

namespace tuxblox {

namespace {

// The published floors, as documented at tuxblox.net/docs.
constexpr int kMinimumKernelMajor = 6;
constexpr int kMinimumKernelMinor = 7;
const char* const kMinimumNvidiaDriver = "418.49.04";
const char* const kVulkanLibrary = "libvulkan.so.1";

// Dot-separated numbers, stopping at anything that is not one -- a kernel release carries a distribution
// suffix ("6.14.2-arch1-1") and a driver version does not, so both end up as just their numbers.
std::vector<int> versionParts(const std::string& text) {
    std::vector<int> parts;
    size_t at = 0;
    while (at < text.size()) {
        if (text[at] < '0' || text[at] > '9') break;
        int value = 0;
        while (at < text.size() && text[at] >= '0' && text[at] <= '9') {
            value = value * 10 + (text[at] - '0');
            ++at;
        }
        parts.push_back(value);
        if (at < text.size() && text[at] == '.') {
            ++at;
            continue;
        }
        break;
    }
    return parts;
}

int partAt(const std::vector<int>& parts, size_t index) {
    return index < parts.size() ? parts[index] : 0;
}

std::string readWholeFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

bool canLoadLibrary(const char* pName) {
    void* pHandle = dlopen(pName, RTLD_LAZY | RTLD_LOCAL);
    if (pHandle == nullptr) return false;
    dlclose(pHandle);
    return true;
}

} // namespace

bool kernelIsOlderThan(const std::string& release, int major, int minor) {
    const std::vector<int> parts = versionParts(release);
    // One number alone is not a kernel version -- "6" could be 6.0 or 6.99, so it proves nothing.
    if (parts.size() < 2) return false;
    if (parts[0] != major) return parts[0] < major;
    return parts[1] < minor;
}

std::string nvidiaDriverVersion(const std::string& procContent) {
    const size_t eol = procContent.find('\n');
    const std::string line = procContent.substr(0, eol == std::string::npos ? procContent.size() : eol);
    if (line.find("NVRM version") == std::string::npos) return "";

    // The first word that is nothing but digits and dots, because the open and the proprietary module
    // put a different number of words in front of the version.
    std::istringstream words(line);
    std::string word;
    while (words >> word) {
        if (word.find('.') == std::string::npos) continue;
        if (word.find_first_not_of("0123456789.") != std::string::npos) continue;
        if (versionParts(word).size() >= 2) return word;
    }
    return "";
}

bool versionIsOlder(const std::string& version, const std::string& minimum) {
    const std::vector<int> have = versionParts(version);
    const std::vector<int> want = versionParts(minimum);
    if (have.empty() || want.empty()) return false;

    const size_t count = have.size() > want.size() ? have.size() : want.size();
    for (size_t i = 0; i < count; ++i) {
        const int a = partAt(have, i);
        const int b = partAt(want, i);
        if (a != b) return a < b;
    }
    return false;
}

std::vector<UnmetRequirement> unmetRequirements(const SystemFacts& facts) {
    std::vector<UnmetRequirement> unmet;

    if (kernelIsOlderThan(facts.kernelRelease, kMinimumKernelMajor, kMinimumKernelMinor)) {
        unmet.push_back({"Linux 6.7 or newer", facts.kernelRelease});
    }

    // Only when there is an NVIDIA driver to read. Mesa does not publish its version anywhere TuxBlox
    // can read without glxinfo installed, so an AMD or Intel machine is never held to a driver floor.
    const std::string nvidia = nvidiaDriverVersion(facts.nvidiaProcContent);
    if (!nvidia.empty() && versionIsOlder(nvidia, kMinimumNvidiaDriver)) {
        unmet.push_back({std::string("NVIDIA driver ") + kMinimumNvidiaDriver + " or newer", nvidia});
    }

    if (!facts.vulkanLoadable) {
        unmet.push_back({"A graphics driver with Vulkan support", ""});
    }

    return unmet;
}

SystemFacts collectSystemFacts() {
    SystemFacts facts;

    utsname name{};
    if (uname(&name) == 0) facts.kernelRelease = name.release;

    facts.nvidiaProcContent = readWholeFile("/proc/driver/nvidia/version");
    // Opened, not used: this asks whether a Vulkan driver is installed at all, which is cheap. Creating
    // an instance to ask whether it works would mean loading the graphics driver into the launcher.
    facts.vulkanLoadable = canLoadLibrary(kVulkanLibrary);

    return facts;
}

std::string unsupportedSystemMessage(const std::vector<UnmetRequirement>& unmet) {
    if (unmet.empty()) return "";

    std::string message = "TuxBlox needs:\n";
    for (const UnmetRequirement& item : unmet) {
        message += "\n\xE2\x80\xA2 " + item.requirement;
        if (!item.found.empty()) message += " \xE2\x80\x94 this computer has " + item.found;
    }
    message += "\n\nSee tuxblox.net/docs for the full requirements.";
    return message;
}

} // namespace tuxblox
