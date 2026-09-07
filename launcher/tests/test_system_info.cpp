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

    // ---- enumerateGpus() / gpuSelectionEnv() ------------------------------
    //
    // This machine has a single GPU, so the multi-GPU behaviour these cover
    // cannot be exercised for real here. The fake sysfs trees below are
    // therefore the only check that a hybrid or multi-seat machine is read
    // correctly -- they stand in for hardware the author cannot test on.

    // Builds one fake "cardN" under `root`: vendor/device files, a driver
    // symlink, and a uevent carrying PCI_SLOT_NAME, exactly as real sysfs
    // lays them out.
    auto makeCard = [](const fs::path& root, const std::string& card,
                       const std::string& vendor, const std::string& device,
                       const std::string& driver, const std::string& slot) {
        fs::path dev = root / card / "device";
        fs::create_directories(dev);
        { std::ofstream(dev / "vendor") << vendor << "\n"; }
        { std::ofstream(dev / "device") << device << "\n"; }
        if (!slot.empty()) {
            std::ofstream(dev / "uevent")
                << "DRIVER=" << driver << "\nPCI_SLOT_NAME=" << slot << "\n";
        }
        if (!driver.empty()) {
            fs::path target = root / "fake_drivers" / driver;
            fs::create_directories(target);
            fs::create_symlink(target, dev / "driver");
        }
    };

    // A fixture in pci.ids' real format: vendor lines flush left, device lines
    // one tab in, subsystem lines two tabs in, comments and a trailing class
    // section -- all of which the parser has to step over correctly.
    fs::path pciIds = fs::temp_directory_path() / "tuxblox_test_pci.ids";
    {
        std::ofstream out(pciIds);
        out << "#\n"
               "#\tList of PCI ID's\n"
               "#\n"
               "1002  Advanced Micro Devices, Inc. [AMD/ATI]\n"
               "\t744c  Navi 31 [Radeon RX 7900 XT/7900 XTX/7900M]\n"
               "\t\t1002 0e3b  Radeon RX 7900 XTX\n"
               "\t9999  A Card With No Bracketed Name\n"
               "8086  Intel Corporation\n"
               "\t9a49  TigerLake-LP GT2 [Iris Xe Graphics]\n"
               "10de  NVIDIA Corporation\n"
               "\t2d04  GB206 [GeForce RTX 5060 Ti]\n"
               "\t2520  GA106M [GeForce RTX 3060 Mobile / Max-Q]\n"
               "# the class section that closes every pci.ids file\n"
               "C 03  Display controller\n"
               "\t00  VGA compatible controller\n";
    }

    // lookupPciDeviceName(): the bracketed marketing name wins over the chip
    // codename -- "GeForce RTX 5060 Ti", not "GB206 [GeForce RTX 5060 Ti]".
    {
        assert(lookupPciDeviceName(pciIds.string(), "0x10de", "0x2d04") == "GeForce RTX 5060 Ti");
        assert(lookupPciDeviceName(pciIds.string(), "0x8086", "0x9a49") == "Iris Xe Graphics");
        assert(lookupPciDeviceName(pciIds.string(), "0x1002", "0x744c") ==
               "Radeon RX 7900 XT/7900 XTX/7900M");
    }

    // lookupPciDeviceName(): a device with no bracketed half keeps its whole
    // name rather than coming back empty.
    {
        assert(lookupPciDeviceName(pciIds.string(), "0x1002", "0x9999") ==
               "A Card With No Bracketed Name");
    }

    // lookupPciDeviceName(): a device id that belongs to a DIFFERENT vendor
    // must not match -- the search has to stop at the next vendor line rather
    // than scanning the rest of the file.
    {
        assert(lookupPciDeviceName(pciIds.string(), "0x1002", "0x2d04").empty());
        assert(lookupPciDeviceName(pciIds.string(), "0x8086", "0x2520").empty());
    }

    // lookupPciDeviceName(): unknown vendor, unknown device, missing file and
    // malformed ids all return "" rather than throwing or half-matching. The
    // subsystem line "1002 0e3b" must not be mistaken for a device either.
    {
        assert(lookupPciDeviceName(pciIds.string(), "0xbeef", "0x1234").empty());
        assert(lookupPciDeviceName(pciIds.string(), "0x10de", "0xffff").empty());
        assert(lookupPciDeviceName("/nonexistent/pci.ids", "0x10de", "0x2d04").empty());
        assert(lookupPciDeviceName(pciIds.string(), "", "").empty());
        assert(lookupPciDeviceName(pciIds.string(), "0x1002", "0x0e3b").empty());
    }

    // enumerateGpus(): missing root -> empty, never throws.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_missing";
        fs::remove_all(root);
        assert(enumerateGpus(root.string(), {}).empty());
    }

    // enumerateGpus(): the hybrid-laptop case -- Intel iGPU on card0, NVIDIA
    // dGPU on card1. Both are listed, in cardN order, each with its own slot.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_hybrid";
        fs::remove_all(root);
        makeCard(root, "card0", "0x8086", "0x9a49", "i915", "0000:00:02.0");
        makeCard(root, "card1", "0x10de", "0x2520", "nvidia", "0000:01:00.0");

        auto gpus = enumerateGpus(root.string(), {});
        assert(gpus.size() == 2);
        assert(gpus[0].pciAddress == "0000:00:02.0");
        assert(gpus[0].vendorId == "0x8086");
        assert(gpus[0].deviceId == "0x9a49");
        assert(gpus[0].driver == "i915");
        assert(gpus[0].label == "Intel (i915)");
        assert(gpus[1].pciAddress == "0000:01:00.0");
        assert(gpus[1].driver == "nvidia");
        assert(gpus[1].label == "NVIDIA (nvidia)");
        fs::remove_all(root);
    }

    // enumerateGpus(): two identical cards would produce the same label, so
    // the PCI slot is appended to BOTH -- a picker with two entries reading
    // "AMD (amdgpu)" would be unusable.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_twins";
        fs::remove_all(root);
        makeCard(root, "card0", "0x1002", "0x744c", "amdgpu", "0000:03:00.0");
        makeCard(root, "card1", "0x1002", "0x744c", "amdgpu", "0000:0a:00.0");

        auto gpus = enumerateGpus(root.string(), {});
        assert(gpus.size() == 2);
        assert(gpus[0].label == "AMD (amdgpu) at 0000:03:00.0");
        assert(gpus[1].label == "AMD (amdgpu) at 0000:0a:00.0");
        assert(gpus[0].label != gpus[1].label);
        fs::remove_all(root);
    }

    // enumerateGpus(): a card with no bound driver, and a card with no
    // readable PCI slot, are both left out -- neither could be selected.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_skipped";
        fs::remove_all(root);
        makeCard(root, "card0", "0x1002", "0x744c", "", "0000:03:00.0"); // no driver
        makeCard(root, "card1", "0x10de", "0x2520", "nvidia", "");       // no slot
        makeCard(root, "card2", "0x8086", "0x9a49", "i915", "0000:00:02.0");

        auto gpus = enumerateGpus(root.string(), {});
        assert(gpus.size() == 1);
        assert(gpus[0].driver == "i915");
        fs::remove_all(root);
    }

    // enumerateGpus(): with a pci.ids available, the picker shows the model
    // name rather than just the vendor -- "NVIDIA GeForce RTX 5060 Ti", not
    // "NVIDIA (nvidia)". The vendor label still leads, because a name like
    // "Iris Xe Graphics" does not otherwise say who made it.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_named";
        fs::remove_all(root);
        makeCard(root, "card0", "0x8086", "0x9a49", "i915", "0000:00:02.0");
        makeCard(root, "card1", "0x10de", "0x2d04", "nvidia", "0000:2b:00.0");

        auto gpus = enumerateGpus(root.string(), {pciIds.string()});
        assert(gpus.size() == 2);
        assert(gpus[0].label == "Intel Iris Xe Graphics");
        assert(gpus[1].label == "NVIDIA GeForce RTX 5060 Ti");
        // The identifying fields are still sysfs', not the database's.
        assert(gpus[1].pciAddress == "0000:2b:00.0");
        assert(gpus[1].driver == "nvidia");
        fs::remove_all(root);
    }

    // enumerateGpus(): a card the database does not list falls back to the
    // vendor-and-driver label rather than showing nothing.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_unlisted";
        fs::remove_all(root);
        makeCard(root, "card0", "0x10de", "0xffff", "nvidia", "0000:01:00.0");

        auto gpus = enumerateGpus(root.string(), {pciIds.string()});
        assert(gpus.size() == 1);
        assert(gpus[0].label == "NVIDIA (nvidia)");
        fs::remove_all(root);
    }

    // enumerateGpus(): two identical cards still have to be told apart, even
    // now that both carry a real model name.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_named_twins";
        fs::remove_all(root);
        makeCard(root, "card0", "0x10de", "0x2d04", "nvidia", "0000:2b:00.0");
        makeCard(root, "card1", "0x10de", "0x2d04", "nvidia", "0000:2c:00.0");

        auto gpus = enumerateGpus(root.string(), {pciIds.string()});
        assert(gpus.size() == 2);
        assert(gpus[0].label == "NVIDIA GeForce RTX 5060 Ti at 0000:2b:00.0");
        assert(gpus[1].label == "NVIDIA GeForce RTX 5060 Ti at 0000:2c:00.0");
        fs::remove_all(root);
    }

    // enumerateGpus(): the first READABLE candidate wins, so a distribution
    // that keeps pci.ids somewhere else still gets names.
    {
        fs::path root = fs::temp_directory_path() / "tuxblox_test_gpus_second_path";
        fs::remove_all(root);
        makeCard(root, "card0", "0x10de", "0x2d04", "nvidia", "0000:2b:00.0");

        auto gpus = enumerateGpus(root.string(), {"/nonexistent/pci.ids", pciIds.string()});
        assert(gpus.size() == 1);
        assert(gpus[0].label == "NVIDIA GeForce RTX 5060 Ti");
        fs::remove_all(root);
    }

    // gpuSelectionEnv(): a Mesa card gets the Mesa variables and none of the
    // NVIDIA ones. MESA_VK_DEVICE_SELECT takes bare hex, no "0x", and
    // DRI_PRIME takes the slot with ':' and '.' replaced by '_'.
    {
        GpuDevice amd;
        amd.pciAddress = "0000:03:00.0";
        amd.vendorId = "0x1002";
        amd.deviceId = "0x744c";
        amd.driver = "amdgpu";
        auto env = gpuSelectionEnv(amd);

        auto has = [&](const std::string& pair) {
            return std::find(env.begin(), env.end(), pair) != env.end();
        };
        assert(has("MESA_VK_DEVICE_SELECT=1002:744c"));
        assert(has("DRI_PRIME=pci-0000_03_00_0"));
        for (const auto& pair : env) assert(pair.rfind("__NV_PRIME", 0) != 0);
    }

    // gpuSelectionEnv(): an NVIDIA card gets the offload variables and no
    // MESA_VK_DEVICE_SELECT, which its proprietary driver ignores anyway.
    {
        GpuDevice nv;
        nv.pciAddress = "0000:01:00.0";
        nv.vendorId = "0x10de";
        nv.deviceId = "0x2520";
        nv.driver = "nvidia";
        auto env = gpuSelectionEnv(nv);

        auto has = [&](const std::string& pair) {
            return std::find(env.begin(), env.end(), pair) != env.end();
        };
        assert(has("__NV_PRIME_RENDER_OFFLOAD=1"));
        assert(has("__VK_LAYER_NV_optimus=NVIDIA_only"));
        assert(has("__GLX_VENDOR_LIBRARY_NAME=nvidia"));
        for (const auto& pair : env) assert(pair.rfind("MESA_VK_DEVICE_SELECT", 0) != 0);
    }

    // gpuSelectionEnv(): a Mesa card whose device id could not be read still
    // gets DRI_PRIME, just not the Vulkan selector -- half the steering is
    // better than none, and an empty id would make a malformed variable.
    {
        GpuDevice partial;
        partial.pciAddress = "0000:03:00.0";
        partial.vendorId = "0x1002";
        partial.driver = "amdgpu";
        auto env = gpuSelectionEnv(partial);
        for (const auto& pair : env) assert(pair.rfind("MESA_VK_DEVICE_SELECT", 0) != 0);
        assert(std::find(env.begin(), env.end(), "DRI_PRIME=pci-0000_03_00_0") != env.end());
    }

    // gpuEnvForSelection(): the Automatic default must emit NOTHING. This is
    // what guarantees an untouched environment for everyone who never opens
    // the picker, so it is asserted rather than assumed.
    {
        std::vector<GpuDevice> gpus;
        GpuDevice nv;
        nv.pciAddress = "0000:01:00.0";
        nv.vendorId = "0x10de";
        nv.driver = "nvidia";
        gpus.push_back(nv);

        assert(gpuEnvForSelection("", gpus).empty());
        assert(!gpuEnvForSelection("0000:01:00.0", gpus).empty());
        // A saved choice naming a card that is no longer present falls back
        // to automatic rather than steering onto something absent.
        assert(gpuEnvForSelection("0000:09:00.0", gpus).empty());
    }

    printf("system_info: all tests passed\n");
    return 0;
}
