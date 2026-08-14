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

#include "system_info.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/utsname.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

std::string parseOsRelease(const std::string& osReleaseContent) {
    std::unordered_map<std::string, std::string> values;
    std::istringstream stream(osReleaseContent);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        values[key] = value;
    }

    auto it = values.find("PRETTY_NAME");
    if (it != values.end() && !it->second.empty()) return it->second;

    it = values.find("NAME");
    if (it != values.end() && !it->second.empty()) {
        std::string result = it->second;
        auto verIt = values.find("VERSION_ID");
        if (verIt != values.end() && !verIt->second.empty()) {
            result += " " + verIt->second;
        }
        return result;
    }
    return "";
}

std::string detectDisplayServer(const GetEnvFn& getenvFn) {
    const char* wayland = getenvFn("WAYLAND_DISPLAY");
    if (wayland && wayland[0] != '\0') return "Wayland";
    const char* display = getenvFn("DISPLAY");
    if (display && display[0] != '\0') return "X11";
    return "Unknown";
}

std::string detectDesktopEnvironment(const GetEnvFn& getenvFn) {
    static const char* kCandidates[] = {"XDG_CURRENT_DESKTOP", "DESKTOP_SESSION", "XDG_SESSION_DESKTOP"};
    for (const char* name : kCandidates) {
        const char* value = getenvFn(name);
        if (value && value[0] != '\0') return value;
    }
    return "Unknown";
}

std::string gpuVendorLabel(const std::string& pciVendorId) {
    if (pciVendorId == "0x10de") return "NVIDIA";
    if (pciVendorId == "0x1002" || pciVendorId == "0x1022") return "AMD";
    if (pciVendorId == "0x8086") return "Intel";
    return "Unknown";
}

std::optional<std::pair<std::string, std::string>> findPrimaryGpu(const std::string& drmRoot) {
    std::error_code ec;
    if (!fs::exists(drmRoot, ec) || ec) return std::nullopt;

    std::vector<fs::path> cardDirs;
    for (const auto& entry : fs::directory_iterator(drmRoot, ec)) {
        if (ec) break;
        if (!entry.is_directory()) continue;
        std::string name = entry.path().filename().string();
        // Only "cardN" entries -- skip "renderD1xx" and any control nodes.
        if (name.rfind("card", 0) != 0 || name.size() <= 4) continue;
        bool allDigits = std::all_of(name.begin() + 4, name.end(),
                                      [](unsigned char c) { return std::isdigit(c) != 0; });
        if (!allDigits) continue;
        cardDirs.push_back(entry.path());
    }
    std::sort(cardDirs.begin(), cardDirs.end());

    for (const auto& cardDir : cardDirs) {
        fs::path devicePath = cardDir / "device";

        std::ifstream vendorFile(devicePath / "vendor");
        if (!vendorFile) continue;
        std::string vendorId;
        std::getline(vendorFile, vendorId);
        while (!vendorId.empty() && (vendorId.back() == '\r' || vendorId.back() == ' ')) vendorId.pop_back();
        if (vendorId.empty()) continue;

        std::error_code driverEc;
        fs::path resolvedDriver = fs::read_symlink(devicePath / "driver", driverEc);
        if (driverEc) continue; // no driver symlink -- device isn't bound to a driver
        std::string driverName = resolvedDriver.filename().string();
        if (driverName.empty()) continue;

        return std::make_pair(vendorId, driverName);
    }
    return std::nullopt;
}

namespace {

// Best-effort: reads /proc/driver/nvidia/version's first line (e.g. "NVRM
// version: NVIDIA UNIX x86_64 Kernel Module  550.107.02  ...") and returns
// the last numeric-leading token, which is the driver version.
std::optional<std::string> readNvidiaProprietaryVersion() {
    std::ifstream in("/proc/driver/nvidia/version");
    if (!in) return std::nullopt;
    std::string line;
    if (!std::getline(in, line)) return std::nullopt;

    std::istringstream stream(line);
    std::string token, lastNumericToken;
    while (stream >> token) {
        if (!token.empty() && std::isdigit(static_cast<unsigned char>(token[0]))) {
            lastNumericToken = token;
        }
    }
    if (lastNumericToken.empty()) return std::nullopt;
    return lastNumericToken;
}

// Best-effort, never a hard dependency: shells out to glxinfo -B only if
// it's already installed, and pulls the Mesa version out of its "OpenGL
// version string" line (e.g. "... Mesa 24.1.5"). nullopt if glxinfo isn't
// on PATH or its output doesn't contain a recognizable Mesa version.
std::optional<std::string> readMesaVersionBestEffort() {
    FILE* pipe = popen("glxinfo -B 2>/dev/null", "r");
    if (!pipe) return std::nullopt;
    std::string output;
    char buf[256];
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    pclose(pipe);

    auto pos = output.find("Mesa ");
    if (pos == std::string::npos) return std::nullopt;
    pos += 5;
    std::string version;
    while (pos < output.size() && (std::isdigit(static_cast<unsigned char>(output[pos])) || output[pos] == '.')) {
        version += output[pos];
        ++pos;
    }
    if (version.empty()) return std::nullopt;
    return version;
}

} // namespace

std::string detectGpu() {
    auto found = findPrimaryGpu("/sys/class/drm");
    if (!found) return "Unknown";
    const std::string& vendorId = found->first;
    const std::string& driverName = found->second;
    std::string vendorLabel = gpuVendorLabel(vendorId);

    if (driverName == "nvidia") {
        auto version = readNvidiaProprietaryVersion();
        if (version) return vendorLabel + " " + *version + " proprietary";
        return vendorLabel + " proprietary (driver: nvidia)";
    }

    // Every other kernel DRM driver on Linux (amdgpu, i915, xe, nouveau,
    // radeon, virtio_gpu, ...) renders through Mesa.
    auto mesaVersion = readMesaVersionBestEffort();
    if (mesaVersion) return "Mesa " + *mesaVersion + " (" + vendorLabel + " " + driverName + ")";
    return vendorLabel + " Mesa (driver: " + driverName + ")";
}

bool detectRootPrivileges() {
    return geteuid() == 0;
}

SystemInfo collectSystemInfo() {
    SystemInfo info;

    std::ifstream osRelease("/etc/os-release");
    if (osRelease) {
        std::ostringstream buf;
        buf << osRelease.rdbuf();
        info.os = parseOsRelease(buf.str());
    }
    if (info.os.empty()) {
        // /etc/os-release is missing entirely on some systems -- fall back
        // to uname(2) rather than reporting nothing.
        struct utsname uts{};
        if (uname(&uts) == 0) {
            info.os = std::string(uts.sysname) + " " + uts.release;
        }
    }

    auto realGetenv = [](const char* name) -> const char* { return std::getenv(name); };
    info.displayServer = detectDisplayServer(realGetenv);
    info.desktopEnvironment = detectDesktopEnvironment(realGetenv);
    info.gpu = detectGpu();
    info.hasRootPrivileges = detectRootPrivileges();
    return info;
}

} // namespace tuxblox
