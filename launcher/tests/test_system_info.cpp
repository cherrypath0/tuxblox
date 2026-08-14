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
#include <cassert>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

int main() {
    using namespace tuxblox;

    // parseOsRelease(): PRETTY_NAME wins when present.
    {
        std::string content =
            "PRETTY_NAME=\"Debian GNU/Linux 12 (bookworm)\"\n"
            "NAME=\"Debian GNU/Linux\"\n"
            "VERSION_ID=\"12\"\n"
            "ID=debian\n";
        assert(parseOsRelease(content) == "Debian GNU/Linux 12 (bookworm)");
    }

    // parseOsRelease(): no PRETTY_NAME -> falls back to "NAME VERSION_ID".
    {
        std::string content = "NAME=\"Fedora Linux\"\nVERSION_ID=\"40\"\n";
        assert(parseOsRelease(content) == "Fedora Linux 40");
    }

    // parseOsRelease(): only NAME (rolling release, no VERSION_ID) -> NAME alone.
    {
        std::string content = "NAME=\"Arch Linux\"\nID=arch\nBUILD_ID=rolling\n";
        assert(parseOsRelease(content) == "Arch Linux");
    }

    // parseOsRelease(): unquoted values are accepted as-is.
    {
        std::string content = "NAME=FreeBSD\nVERSION_ID=15.1\n";
        assert(parseOsRelease(content) == "FreeBSD 15.1");
    }

    // parseOsRelease(): empty/irrelevant content -> "".
    {
        assert(parseOsRelease("") == "");
        assert(parseOsRelease("# just a comment\nID=foo\n") == "");
    }

    // detectDisplayServer(): Wayland takes priority even if DISPLAY (XWayland) is also set.
    {
        auto env = [](const char* name) -> const char* {
            if (std::strcmp(name, "WAYLAND_DISPLAY") == 0) return "wayland-0";
            if (std::strcmp(name, "DISPLAY") == 0) return ":0";
            return nullptr;
        };
        assert(detectDisplayServer(env) == "Wayland");
    }

    // detectDisplayServer(): DISPLAY only -> X11.
    {
        auto env = [](const char* name) -> const char* {
            return std::strcmp(name, "DISPLAY") == 0 ? ":0" : nullptr;
        };
        assert(detectDisplayServer(env) == "X11");
    }

    // detectDisplayServer(): neither set -> Unknown.
    {
        auto env = [](const char*) -> const char* { return nullptr; };
        assert(detectDisplayServer(env) == "Unknown");
    }

    // detectDesktopEnvironment(): XDG_CURRENT_DESKTOP wins when several are set.
    {
        auto env = [](const char* name) -> const char* {
            if (std::strcmp(name, "XDG_CURRENT_DESKTOP") == 0) return "GNOME";
            if (std::strcmp(name, "DESKTOP_SESSION") == 0) return "gnome-classic";
            return nullptr;
        };
        assert(detectDesktopEnvironment(env) == "GNOME");
    }

    // detectDesktopEnvironment(): falls back to DESKTOP_SESSION.
    {
        auto env = [](const char* name) -> const char* {
            return std::strcmp(name, "DESKTOP_SESSION") == 0 ? "i3" : nullptr;
        };
        assert(detectDesktopEnvironment(env) == "i3");
    }

    // detectDesktopEnvironment(): falls back to XDG_SESSION_DESKTOP.
    {
        auto env = [](const char* name) -> const char* {
            return std::strcmp(name, "XDG_SESSION_DESKTOP") == 0 ? "sway" : nullptr;
        };
        assert(detectDesktopEnvironment(env) == "sway");
    }

    // detectDesktopEnvironment(): nothing set -> Unknown.
    {
        auto env = [](const char*) -> const char* { return nullptr; };
        assert(detectDesktopEnvironment(env) == "Unknown");
    }

    // gpuVendorLabel(): known PCI vendor IDs.
    {
        assert(gpuVendorLabel("0x10de") == "NVIDIA");
        assert(gpuVendorLabel("0x1002") == "AMD");
        assert(gpuVendorLabel("0x8086") == "Intel");
        assert(gpuVendorLabel("0xffff") == "Unknown");
        assert(gpuVendorLabel("") == "Unknown");
    }

    // findPrimaryGpu(): missing root directory -> nullopt.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_sysinfo_no_such_drm_root";
        fs::remove_all(root);
        assert(!findPrimaryGpu(root.string()).has_value());
    }

    // findPrimaryGpu(): a well-formed fake sysfs tree resolves vendor+driver.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_sysinfo_drm_root_ok";
        fs::remove_all(root);
        fs::create_directories(root / "card0" / "device");
        {
            std::ofstream out(root / "card0" / "device" / "vendor");
            out << "0x10de\n";
        }
        // Real sysfs exposes the bound driver as a symlink to
        // .../drivers/<name>; only the basename matters to us.
        fs::create_directories(root / "fake_driver_target" / "nvidia");
        fs::create_symlink(root / "fake_driver_target" / "nvidia", root / "card0" / "device" / "driver");

        auto found = findPrimaryGpu(root.string());
        assert(found.has_value());
        assert(found->first == "0x10de");
        assert(found->second == "nvidia");
        fs::remove_all(root);
    }

    // findPrimaryGpu(): a card with a vendor file but no bound driver (e.g.
    // an unbound/disabled device) is skipped rather than reported.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_sysinfo_drm_root_unbound";
        fs::remove_all(root);
        fs::create_directories(root / "card0" / "device");
        {
            std::ofstream out(root / "card0" / "device" / "vendor");
            out << "0x1002\n";
        }
        // No "driver" symlink created for card0 -- unbound.
        assert(!findPrimaryGpu(root.string()).has_value());
        fs::remove_all(root);
    }

    // detectRootPrivileges() / collectSystemInfo(): smoke-tested for real --
    // must never throw, and every field must be populated with *something*
    // (even if "Unknown" on whatever machine runs this test).
    {
        (void)detectRootPrivileges(); // just must not crash
        SystemInfo info = collectSystemInfo();
        assert(!info.displayServer.empty());
        assert(!info.desktopEnvironment.empty());
        assert(!info.gpu.empty());
        // os may legitimately be "" only if /etc/os-release is entirely
        // absent AND uname() somehow fails too -- not expected on any real
        // Linux test runner, so this should hold in practice.
    }

    printf("system_info: all tests passed\n");
    return 0;
}
