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

// True if running as root (via sudo or a root login) -- both collapse to
// EUID 0, so a single check covers "ran with sudo or as root".
bool detectRootPrivileges();

// Gathers every field above from the real system. Best-effort: never
// throws, individual fields fall back to "Unknown" rather than failing
// the whole call.
SystemInfo collectSystemInfo();

} // namespace tuxblox
