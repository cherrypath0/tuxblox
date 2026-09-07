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

namespace {

// Trims trailing CR/space/tab, which sysfs files carry after the newline
// std::getline already removed.
std::string trimTrailing(std::string value) {
    while (!value.empty() && (value.back() == '\r' || value.back() == ' ' || value.back() == '\t')) {
        value.pop_back();
    }
    return value;
}

std::string readFirstLine(const fs::path& path) {
    std::ifstream in(path);
    if (!in) return "";
    std::string line;
    std::getline(in, line);
    return trimTrailing(line);
}

// Real sysfs exposes a card's PCI slot two ways: PCI_SLOT_NAME in the
// device's uevent, and the basename of the "device" symlink. uevent is tried
// first because it is a plain file that says what it means; the symlink is
// the fallback for a kernel that omits the key.
std::string readPciSlot(const fs::path& devicePath) {
    std::ifstream uevent(devicePath / "uevent");
    if (uevent) {
        std::string line;
        while (std::getline(uevent, line)) {
            const std::string key = "PCI_SLOT_NAME=";
            if (line.rfind(key, 0) == 0) {
                std::string slot = trimTrailing(line.substr(key.size()));
                if (!slot.empty()) return slot;
            }
        }
    }

    std::error_code ec;
    if (fs::is_symlink(devicePath, ec) && !ec) {
        fs::path resolved = fs::read_symlink(devicePath, ec);
        if (!ec) {
            std::string slot = resolved.filename().string();
            if (!slot.empty()) return slot;
        }
    }
    return "";
}

// Every kernel DRM driver other than NVIDIA's proprietary one renders through
// Mesa (amdgpu, i915, xe, nouveau, radeon, virtio_gpu, ...), which is the same
// split detectGpu() above already makes.
bool isNvidiaProprietary(const std::string& driver) {
    return driver == "nvidia";
}

// "0x10de" and "10de" both normalise to "10de", which is what pci.ids uses.
std::string bareHexId(const std::string& id) {
    std::string bare = id.rfind("0x", 0) == 0 ? id.substr(2) : id;
    std::transform(bare.begin(), bare.end(), bare.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return bare;
}

} // namespace

std::vector<std::string> defaultPciIdsPaths() {
    return {
        "/usr/share/hwdata/pci.ids",  // Arch, Fedora, openSUSE (hwdata)
        "/usr/share/misc/pci.ids",    // Debian, Ubuntu (pciutils)
        "/usr/share/pci.ids",         // older layouts and some minimal distributions
    };
}

std::string lookupPciDeviceName(const std::string& pciIdsPath, const std::string& vendorId,
                                 const std::string& deviceId) {
    const std::string wantVendor = bareHexId(vendorId);
    const std::string wantDevice = bareHexId(deviceId);
    if (wantVendor.empty() || wantDevice.empty()) return "";

    std::ifstream in(pciIdsPath);
    if (!in) return "";

    // pci.ids nests by indentation: vendors flush left, their devices one tab
    // in, and each device's subsystems two tabs in. Depth is the only thing
    // separating a device id from a subsystem id, several of which repeat ids
    // that also exist as devices, so it is what the parse keys on.
    bool inVendor = false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;

        if (line[0] != '\t') {
            // A flush-left line is a new vendor (or the trailing "C 03  ..."
            // class section). Either way the vendor we wanted has ended.
            if (inVendor) return "";
            // "10de  NVIDIA Corporation" -- the id, then two spaces.
            if (line.rfind(wantVendor + "  ", 0) == 0) inVendor = true;
            continue;
        }
        if (!inVendor) continue;
        if (line.size() < 2 || line[1] == '\t') continue; // a subsystem, not a device

        std::string entry = line.substr(1);
        if (entry.rfind(wantDevice + "  ", 0) != 0) continue;

        std::string name = trimTrailing(entry.substr(wantDevice.size() + 2));
        // "GB206 [GeForce RTX 5060 Ti]" -- the bracketed half is the name a
        // person recognises; the part before it is the chip codename.
        auto open = name.find('[');
        auto close = name.rfind(']');
        if (open != std::string::npos && close != std::string::npos && close > open + 1) {
            return name.substr(open + 1, close - open - 1);
        }
        return name;
    }
    return "";
}

std::vector<GpuDevice> enumerateGpus(const std::string& drmRoot) {
    return enumerateGpus(drmRoot, defaultPciIdsPaths());
}

std::vector<GpuDevice> enumerateGpus(const std::string& drmRoot,
                                      const std::vector<std::string>& pciIdsCandidates) {
    std::error_code ec;
    if (!fs::exists(drmRoot, ec) || ec) return {};

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
    // Sorted so the picker lists cards in the same order every run. Plain
    // path order puts card10 before card2, which is cosmetic here: the saved
    // choice is a PCI slot, so ordering never changes what is selected.
    std::sort(cardDirs.begin(), cardDirs.end());

    // Resolved once for the whole sweep rather than per card: the file is
    // ~1.6MB and every card would otherwise re-open and re-scan it.
    std::string pciIdsPath;
    for (const auto& candidate : pciIdsCandidates) {
        std::ifstream probe(candidate);
        if (probe) {
            pciIdsPath = candidate;
            break;
        }
    }

    std::vector<GpuDevice> gpus;
    for (const auto& cardDir : cardDirs) {
        fs::path devicePath = cardDir / "device";

        GpuDevice gpu;
        gpu.vendorId = readFirstLine(devicePath / "vendor");
        if (gpu.vendorId.empty()) continue;

        std::error_code driverEc;
        fs::path resolvedDriver = fs::read_symlink(devicePath / "driver", driverEc);
        if (driverEc) continue; // unbound device -- nothing would render on it
        gpu.driver = resolvedDriver.filename().string();
        if (gpu.driver.empty()) continue;

        gpu.pciAddress = readPciSlot(devicePath);
        if (gpu.pciAddress.empty()) continue; // no stable way to select it later

        gpu.deviceId = readFirstLine(devicePath / "device");

        // "NVIDIA GeForce RTX 5060 Ti" when the model is known, falling back
        // to "NVIDIA (nvidia)" when pci.ids is absent or does not list the
        // card. The vendor leads either way: a name like "Iris Xe Graphics"
        // does not otherwise say who made it.
        const std::string vendorLabel = gpuVendorLabel(gpu.vendorId);
        std::string model;
        if (!pciIdsPath.empty()) {
            model = lookupPciDeviceName(pciIdsPath, gpu.vendorId, gpu.deviceId);
        }
        if (model.empty()) {
            gpu.label = vendorLabel + " (" + gpu.driver + ")";
        } else if (model.rfind(vendorLabel, 0) == 0) {
            gpu.label = model; // already names its vendor -- don't say it twice
        } else {
            gpu.label = vendorLabel + " " + model;
        }
        gpus.push_back(std::move(gpu));
    }

    // Disambiguate cards that would otherwise read identically. Done after the
    // fact rather than by always including the slot, so the common hybrid case
    // stays readable ("NVIDIA (nvidia)", not "NVIDIA (nvidia) at 0000:01:00.0").
    //
    // The counts are taken from a snapshot rather than from gpus itself:
    // appending to one card's label as we go would leave the next card with
    // the only remaining copy of the original, look unique, and keep the
    // ambiguous name -- so exactly one of two identical cards would be labelled.
    std::unordered_map<std::string, int> labelCounts;
    for (const auto& gpu : gpus) labelCounts[gpu.label]++;
    for (auto& gpu : gpus) {
        if (labelCounts[gpu.label] > 1) gpu.label += " at " + gpu.pciAddress;
    }
    return gpus;
}

std::vector<std::string> gpuSelectionEnv(const GpuDevice& gpu) {
    std::vector<std::string> env;
    if (gpu.pciAddress.empty()) return env;

    if (isNvidiaProprietary(gpu.driver)) {
        // The trio NVIDIA's own PRIME render-offload documentation specifies.
        // __VK_LAYER_NV_optimus is the one that matters to Roblox, which
        // renders through Vulkan; the GLX variable covers anything in the
        // prefix that still goes through OpenGL.
        env.push_back("__NV_PRIME_RENDER_OFFLOAD=1");
        env.push_back("__VK_LAYER_NV_optimus=NVIDIA_only");
        env.push_back("__GLX_VENDOR_LIBRARY_NAME=nvidia");
        return env;
    }

    // MESA_VK_DEVICE_SELECT wants bare hex ids, "1002:744c", where sysfs
    // spells them "0x1002". Skipped entirely when the device id could not be
    // read, since a half-formed value would select nothing and hide the
    // reason.
    auto stripHexPrefix = [](const std::string& id) {
        return id.rfind("0x", 0) == 0 ? id.substr(2) : id;
    };
    if (!gpu.vendorId.empty() && !gpu.deviceId.empty()) {
        env.push_back("MESA_VK_DEVICE_SELECT=" + stripHexPrefix(gpu.vendorId) + ":" +
                      stripHexPrefix(gpu.deviceId));
    }

    // DRI_PRIME's pci- form spells the slot with underscores: "0000:03:00.0"
    // becomes "pci-0000_03_00_0".
    std::string slot = gpu.pciAddress;
    std::replace(slot.begin(), slot.end(), ':', '_');
    std::replace(slot.begin(), slot.end(), '.', '_');
    env.push_back("DRI_PRIME=pci-" + slot);
    return env;
}

std::vector<std::string> gpuEnvForSelection(const std::string& pciAddress,
                                             const std::vector<GpuDevice>& gpus) {
    if (pciAddress.empty()) return {};
    for (const auto& gpu : gpus) {
        if (gpu.pciAddress == pciAddress) return gpuSelectionEnv(gpu);
    }
    return {};
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
