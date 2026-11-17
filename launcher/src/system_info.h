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
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace tuxblox {

struct SystemInfo {
    std::string os;                  // e.g. "Debian GNU/Linux 12 (bookworm)", "Arch Linux"
    std::string displayServer;       // "Wayland", "X11", or "Unknown"
    std::string desktopEnvironment;  // e.g. "GNOME", "KDE", "Hyprland", "sway", "i3", or "Unknown"
    std::string gpu;                 // e.g. "NVIDIA 550.107.02 proprietary", "AMD Mesa 24.1.x (amdgpu)"
    bool hasRootPrivileges = false;
};

using GetEnvFn = std::function<const char*(const char*)>;

// Parses /etc/os-release-style "KEY=VALUE" content (values optionally
// wrapped in double quotes, '#' comment lines ignored) and returns, in
// order of preference: PRETTY_NAME; else NAME plus VERSION_ID if present;
// else NAME alone; else "" if none of those keys are present.
std::string parseOsRelease(const std::string& osReleaseContent);

// $WAYLAND_DISPLAY set (even if $DISPLAY is also set, e.g. under XWayland)
// -> "Wayland"; else $DISPLAY set -> "X11"; else "Unknown".
std::string detectDisplayServer(const GetEnvFn& getenvFn);

// $XDG_CURRENT_DESKTOP, else $DESKTOP_SESSION, else $XDG_SESSION_DESKTOP,
// else "Unknown". First non-empty one wins, checked in that order.
std::string detectDesktopEnvironment(const GetEnvFn& getenvFn);

// Maps a PCI vendor ID, as read from /sys/class/drm/*/device/vendor (e.g.
// "0x10de"), to a human-readable label. Unrecognized/empty -> "Unknown".
std::string gpuVendorLabel(const std::string& pciVendorId);

// Walks `drmRoot` (normally "/sys/class/drm") for the first cardN entry
// that has both a readable device/vendor file and a device/driver symlink,
// returning (vendorId, driverName) e.g. ("0x10de", "nvidia"). nullopt if
// drmRoot doesn't exist or no card has both. Injectable root for testing.
std::optional<std::pair<std::string, std::string>> findPrimaryGpu(const std::string& drmRoot);

// Best-effort whole-system GPU + driver string, built on findPrimaryGpu():
// enriches an NVIDIA card with /proc/driver/nvidia/version's version
// number, and a Mesa-backed card with `glxinfo -B`'s Mesa version *only*
// if glxinfo is already installed (never a hard dependency). "Unknown" if
// nothing could be determined.
std::string detectGpu();

// One graphics card the machine has, as the picker offers it.
struct GpuDevice {
    // The card's PCI slot, e.g. "0000:01:00.0". This is the identity stored
    // in settings, deliberately not the cardN index: indices are assigned in
    // probe order and can change between boots or driver updates, which would
    // silently repoint a saved choice at a different card. A card with no
    // readable slot is left out of the list entirely, since nothing could
    // reliably select it again later.
    std::string pciAddress;
    std::string vendorId; // "0x10de"
    std::string deviceId; // "0x2684", empty if unreadable
    std::string driver;   // "nvidia", "amdgpu", "i915", ...
    // What the picker shows, e.g. "NVIDIA (nvidia)". Two cards that would
    // otherwise read identically (a machine with two of the same card) get
    // their PCI slot appended, so every entry in one list is distinguishable.
    std::string label;
};

// The model name for a PCI vendor:device pair, read from a pci.ids database
// ("0x10de", "0x2d04" -> "GeForce RTX 5060 Ti"). Empty when the file is
// missing or has no entry for the pair.
//
// pci.ids spells a device as "GB206 [GeForce RTX 5060 Ti]": a chip codename
// followed by the marketing name in brackets. The bracketed half is what a
// person recognises as their graphics card, so it is preferred when present,
// and the whole string used when it is not.
std::string lookupPciDeviceName(const std::string& pciIdsPath, const std::string& vendorId,
                                 const std::string& deviceId);

// The pci.ids locations shipped by the common distributions, in search order.
// The database comes from hwdata/pciutils and is very widely installed but not
// guaranteed, which is why every caller tolerates it being absent.
std::vector<std::string> defaultPciIdsPaths();

// Every DRM card under `drmRoot` ("/sys/class/drm") that is bound to a driver
// and has a readable PCI slot, ordered by cardN so the list is stable between
// runs. Injectable root for testing, like findPrimaryGpu above. Never throws:
// a missing root or an unreadable card yields a shorter list, not an error.
std::vector<GpuDevice> enumerateGpus(const std::string& drmRoot);

// As above, but reading model names from the first readable path in
// `pciIdsCandidates` instead of the system's own. Pass an empty list to get
// the vendor-and-driver fallback labels with no database at all -- which is
// also what a machine without pci.ids installed produces.
std::vector<GpuDevice> enumerateGpus(const std::string& drmRoot,
                                      const std::vector<std::string>& pciIdsCandidates);

// The Vulkan driver directories the loader itself searches, most specific
// first, so a driver file someone installed for themselves comes first.
std::vector<std::string> defaultVulkanIcdDirs();

// Every Vulkan driver file under `icdDirs` that belongs to `driver`, as
// full paths in search order. A driver file is named after the driver that
// installs it rather than after the kernel module that binds the card, so
// the two are mapped by hand. A card whose driver has no known Vulkan
// counterpart gives an empty list, which callers must read as "leave the
// loader alone" and never as "this machine has no Vulkan driver".
std::vector<std::string> vulkanIcdFiles(const std::string& driver,
                                         const std::vector<std::string>& icdDirs);

// The environment that steers rendering onto `gpu`, as "VAR=VALUE" pairs,
// for a machine whose only card is `gpu`. The overload below is the one
// that knows what else is installed.
std::vector<std::string> gpuSelectionEnv(const GpuDevice& gpu);

// As above, for `gpu` chosen out of the machine's `gpus`, reading Vulkan
// driver files from `icdDirs`.
//
// Which variables are emitted depends on the driver, and the two sets are
// disjoint: MESA_VK_DEVICE_SELECT does nothing to the NVIDIA proprietary
// Vulkan driver, and the __NV_PRIME/__VK_LAYER_NV_optimus variables do
// nothing to Mesa. Emitting the wrong set is not merely useless, it is
// indistinguishable from the feature being broken, so the driver decides.
//
// Steering *away* from a card needs the whole machine, not just the chosen
// one. MESA_VK_DEVICE_SELECT only orders Mesa's own devices and cannot
// remove a device another driver provides, and DRI_PRIME is an OpenGL
// mechanism that does nothing to Vulkan at all -- so on a hybrid laptop
// neither of them can keep the game off the discrete card. Hiding an
// NVIDIA card takes __VK_LAYER_NV_optimus=non_NVIDIA_only, and the Vulkan
// loader is pinned to the chosen card's own driver files on top, which is
// the only part of this that no vendor's driver can ignore.
std::vector<std::string> gpuSelectionEnv(const GpuDevice& gpu,
                                          const std::vector<GpuDevice>& gpus,
                                          const std::vector<std::string>& icdDirs);

// gpuSelectionEnv() for whichever entry of `gpus` has this PCI slot. Empty
// when `pciAddress` is empty (the "Automatic" default, which must leave the
// environment exactly as it found it) or names a card that is no longer
// present -- an unplugged or renumbered card falls back to automatic rather
// than forcing rendering onto something that isn't there.
std::vector<std::string> gpuEnvForSelection(const std::string& pciAddress,
                                             const std::vector<GpuDevice>& gpus);

// True if running as root (via sudo or a root login) -- both collapse to
// EUID 0, so a single check covers "ran with sudo or as root".
bool detectRootPrivileges();

// Gathers every field above from the real system. Best-effort: never
// throws, individual fields fall back to "Unknown" rather than failing
// the whole call.
SystemInfo collectSystemInfo();

} // namespace tuxblox
