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

#include "wine_shortcut_export.h"
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

static std::string readAll(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static void writeAll(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << s;
}

// Cheap, dependency-free proxy for "would GLib's key-file parser (what GIO
// uses under DesktopAppInfo::new_from_filename) accept this file": scans for
// a backslash not followed by one of the spec's five defined value escapes
// (\s \n \t \r \\). That's precisely the shape of Finding 1's bug -- writing
// an unescaped path back into Exec= left bare `\u`-style sequences that made
// GIO refuse to parse the whole file, not just that key.
static bool looksLikeValidKeyFileValue(const std::string& contents) {
    for (size_t i = 0; i < contents.size(); ++i) {
        if (contents[i] != '\\') continue;
        if (i + 1 >= contents.size()) return false; // trailing lone backslash
        const char next = contents[i + 1];
        if (next != 's' && next != 'n' && next != 't' && next != 'r' && next != '\\') return false;
        ++i; // skip the escaped character
    }
    return true;
}

// A proton_shortcuts entry exactly as winemenubuilder writes it: four literal
// backslashes per separator, and a trailing empty "" argument.
static std::string sourceEntry(const std::string& name, const std::string& exeLeaf,
                               const std::string& icon) {
    return "[Desktop Entry]\n"
           "Name=" + name + "\n"
           "Exec=\"C:\\\\\\\\users\\\\\\\\user\\\\\\\\AppData\\\\\\\\Local\\\\\\\\Roblox\\\\\\\\Versions"
           "\\\\\\\\version-abc\\\\\\\\" + exeLeaf + "\" \"\"\n"
           "Type=Application\n"
           "StartupNotify=true\n"
           "Icon=" + icon + "\n"
           "StartupWMClass=" + exeLeaf + "\n";
}

int main() {
    using namespace tuxblox;

    // Four literal backslashes collapse to one.
    assert(unescapeWinemenubuilderPath("C:\\\\\\\\users\\\\\\\\user") == "C:\\users\\user");
    assert(unescapeWinemenubuilderPath("no backslashes") == "no backslashes");

    assert(exeFromDesktopExecLine("\"C:\\\\\\\\a\\\\\\\\B.exe\" \"\"") == "C:\\a\\B.exe");
    assert(exeFromDesktopExecLine("no quotes here").empty());
    assert(exeFromDesktopExecLine("").empty());

    // quotedExecValue() is the un-unescaped counterpart: it must return the
    // exact same escaped (four-backslash) bytes that were in the source
    // Exec= line, since that's what gets written back into the new one --
    // see exportPrefixShortcutsTo()'s use of it.
    assert(quotedExecValue("\"C:\\\\\\\\a\\\\\\\\B.exe\" \"\"") == "C:\\\\\\\\a\\\\\\\\B.exe");
    assert(quotedExecValue("no quotes here").empty());
    assert(quotedExecValue("").empty());

    const fs::path tmp = fs::temp_directory_path() / "tuxblox_shortcut_export_test";
    fs::remove_all(tmp);

    const fs::path installDir = tmp / "install";
    const fs::path shortcuts = installDir / "runtime" / "pfx" / "drive_c" / "proton_shortcuts";
    const fs::path appsDir = tmp / "applications";
    const fs::path iconsDir = tmp / "icons" / "hicolor";
    fs::create_directories(appsDir);

    writeAll(shortcuts / "Roblox Studio.desktop",
             sourceEntry("Roblox Studio", "RobloxStudioBeta.exe", "393D_RobloxStudioBeta.0"));
    writeAll(shortcuts / "Roblox Player.desktop",
             sourceEntry("Roblox Player", "RobloxPlayerBeta.exe", "393D_RobloxPlayerBeta.0"));
    // Not Roblox -- must be skipped, not published to the user's app menu.
    writeAll(shortcuts / "Notepad.desktop",
             sourceEntry("Notepad", "notepad.exe", "notepad.0"));

    writeAll(shortcuts / "icons" / "48x48" / "apps" / "393D_RobloxStudioBeta.0.png", "PNG48");
    writeAll(shortcuts / "icons" / "256x256" / "apps" / "393D_RobloxStudioBeta.0.png", "PNG256");

    exportPrefixShortcutsTo(installDir.string(), "/opt/tuxblox/TuxBloxLauncher",
                            appsDir.string(), iconsDir.string());

    assert(fs::exists(appsDir / "tuxblox-roblox-studio.desktop"));
    assert(fs::exists(appsDir / "tuxblox-roblox-player.desktop"));
    assert(!fs::exists(appsDir / "tuxblox-roblox-notepad.desktop"));

    const std::string studio = readAll(appsDir / "tuxblox-roblox-studio.desktop");
    assert(studio.find("Name=Roblox Studio\n") != std::string::npos);
    assert(studio.find("Comment=via TuxBlox\n") != std::string::npos);
    // Finding 1 regression pin: the Exec= value must be written back in its
    // still-ESCAPED (winemenubuilder four-backslash) form -- the same bytes
    // that were in the source entry's own Exec= line -- not the human-
    // readable single-backslash path. A single backslash here is `\u`, an
    // invalid key-file escape, and GIO refuses to load the *entire* file
    // over it (not just this key). See wine_shortcut_export.cpp's comment on
    // escapedExe for the reasoning; this used to assert exactly the broken
    // single-backslash bytes, pinning the bug instead of catching it.
    assert(studio.find(
               "Exec=\"/opt/tuxblox/TuxBloxLauncher\" --run-exe "
               "\"C:\\\\\\\\users\\\\\\\\user\\\\\\\\AppData\\\\\\\\Local\\\\\\\\Roblox\\\\\\\\Versions"
               "\\\\\\\\version-abc\\\\\\\\RobloxStudioBeta.exe\"\n")
           != std::string::npos);
    // Cheap stand-in for "GIO's key-file parser accepts this file": every
    // backslash in a Desktop Entry value must begin one of the five defined
    // escapes (\s \n \t \r \\); anything else -- like the bare `\u` the
    // original bug produced -- is what made GIO refuse the whole file. This
    // is checked without linking GLib/GIO or shelling out to python3, so it
    // runs everywhere ctest does; the interactive python3+GObject-Introspection
    // check in the design doc is the authoritative, heavier proof against the
    // real library.
    assert(looksLikeValidKeyFileValue(studio));
    // Icons are renamed to a deterministic, TuxBlox-owned name so uninstall can
    // remove exactly what we installed without touching another prefix's icons.
    assert(studio.find("Icon=tuxblox-roblox-studio\n") != std::string::npos);
    assert(readAll(iconsDir / "48x48" / "apps" / "tuxblox-roblox-studio.png") == "PNG48");
    assert(readAll(iconsDir / "256x256" / "apps" / "tuxblox-roblox-studio.png") == "PNG256");

    // No icon in the source tree -> fall back to the TuxBlox icon.
    const std::string player = readAll(appsDir / "tuxblox-roblox-player.desktop");
    assert(player.find("Icon=tuxblox\n") != std::string::npos);

    // Pruning: a source entry that disappears takes its export with it.
    fs::remove(shortcuts / "Roblox Player.desktop");
    exportPrefixShortcutsTo(installDir.string(), "/opt/tuxblox/TuxBloxLauncher",
                            appsDir.string(), iconsDir.string());
    assert(!fs::exists(appsDir / "tuxblox-roblox-player.desktop"));
    assert(fs::exists(appsDir / "tuxblox-roblox-studio.desktop"));

    // A missing prefix is a no-op, not a crash, and must not delete the
    // launcher's own entry.
    writeAll(appsDir / "tuxblox-launcher.desktop", "[Desktop Entry]\n");
    exportPrefixShortcutsTo((tmp / "nonexistent").string(), "/opt/tuxblox/TuxBloxLauncher",
                            appsDir.string(), iconsDir.string());
    assert(fs::exists(appsDir / "tuxblox-launcher.desktop"));

    // Correction to the brief: exportPrefixShortcutsTo() must create appsDir
    // itself rather than assume the caller already did. The real call path
    // (watch_launch.cpp, after a session ends) has no reason to have created
    // ~/.local/share/applications first.
    {
        const fs::path freshTmp = tmp / "fresh";
        const fs::path freshInstall = freshTmp / "install";
        const fs::path freshShortcuts =
            freshInstall / "runtime" / "pfx" / "drive_c" / "proton_shortcuts";
        const fs::path freshAppsDir = freshTmp / "does" / "not" / "exist" / "applications";
        const fs::path freshIconsDir = freshTmp / "icons" / "hicolor";

        writeAll(freshShortcuts / "Roblox Studio.desktop",
                 sourceEntry("Roblox Studio", "RobloxStudioBeta.exe", "393D_RobloxStudioBeta.0"));

        assert(!fs::exists(freshAppsDir));
        exportPrefixShortcutsTo(freshInstall.string(), "/opt/tuxblox/TuxBloxLauncher",
                                freshAppsDir.string(), freshIconsDir.string());
        assert(fs::exists(freshAppsDir / "tuxblox-roblox-studio.desktop"));
    }

    fs::remove_all(tmp);
    std::printf("wine_shortcut_export: all tests passed\n");
    return 0;
}
