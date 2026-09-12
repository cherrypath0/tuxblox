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

#include "prefix/prefix.h"
#include "prefix/registry.h"
#include "embedded_data.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <random>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#include <fcntl.h>
#include <fnmatch.h>
#include <sys/wait.h>
#include <unistd.h>

// Third Parties
#include "third_party/json.hpp"

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

// Files copied rather than symlinked into the prefix, so an installer that
// overwrites one does not write through into the shared TuxBlox install.
//
// ntdll.dll is deliberately not in this list, though Proton copies it. The
// loader maps ntdll straight out of the TuxBlox install, while \KnownDlls\ntdll.dll
// is a section over the copy in system32; a copy makes those two different
// files, and the section then describes an image that is nowhere in memory.
// Linking it keeps them one file, which is what Windows has.
const std::string DefaultDllCopyPatterns =
    "d3dcompiler_*.dll,d3dx*.dll,"
    "atl.dll,atl1*.dll,concrt*.dll,"
    "msvcp1*.dll,msvcp6*.dll,msvcp7*.dll,msvcp_win.dll,"
    "msvcr1*.dll,msvcr7*.dll,msvcrt*.dll,"
    "vcamp1*.dll,vccorlib1*.dll,vcomp1*.dll,vcruntime1*.dll,ucrtbase.dll,"
    // comctl32 exists twice, in system32 and as comctl32_v6 in winsxs.
    "comctl32.dll,"
    // Roblox's anti-cheat loads the official loader.
    "vulkan-1.dll";

std::string envOrEmpty(const char *pName) {
    const char *pValue = std::getenv(pName);
    return pValue != nullptr ? std::string(pValue) : std::string();
}

// Runs a host program and returns its standard output. The loader environment
// is stripped: a LD_LIBRARY_PATH pointing at bundled libraries makes host
// binaries die on a version mismatch instead of answering.
std::string runHostCmd(const std::vector<std::string>& command) {
    int pipeFds[2];
    if (::pipe(pipeFds) != 0) {
        return "";
    }

    const pid_t child = ::fork();
    if (child < 0) {
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        return "";
    }
    if (child == 0) {
        ::dup2(pipeFds[1], STDOUT_FILENO);
        ::close(pipeFds[0]);
        ::close(pipeFds[1]);
        const int devNull = ::open("/dev/null", O_WRONLY);
        if (devNull >= 0) {
            ::dup2(devNull, STDERR_FILENO);
        }
        ::unsetenv("LD_LIBRARY_PATH");
        ::unsetenv("LD_PRELOAD");

        std::vector<std::string> storage = command;
        std::vector<char *> argv;
        for (std::string& entry : storage) {
            argv.push_back(entry.data());
        }
        argv.push_back(nullptr);
        ::execvp(argv[0], argv.data());
        ::_exit(127);
    }

    ::close(pipeFds[1]);
    std::string output;
    char buffer[512];
    ssize_t got = 0;
    while ((got = ::read(pipeFds[0], buffer, sizeof(buffer))) > 0) {
        output.append(buffer, static_cast<size_t>(got));
    }
    ::close(pipeFds[0]);

    int status = 0;
    while (::waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        return "";
    }
    return output;
}

std::string trimmed(const std::string& text) {
    const size_t first = text.find_first_not_of(" \t\r\n'");
    if (first == std::string::npos) {
        return "";
    }
    const size_t last = text.find_last_not_of(" \t\r\n'");
    return text.substr(first, last - first + 1);
}

bool matchesAnyPattern(const std::string& name, const std::vector<std::string>& patterns) {
    for (const std::string& pattern : patterns) {
        if (::fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> splitCommas(const std::string& text) {
    std::vector<std::string> parts;
    std::stringstream stream(text);
    std::string part;
    while (std::getline(stream, part, ',')) {
        if (!part.empty()) {
            parts.push_back(part);
        }
    }
    return parts;
}

// Guards against a path bug writing outside the prefix. The template is full
// of symlinks, and a mistake resolving one used to land whole directories of
// DLLs beside the prefix instead of inside it.
bool isInside(const fs::path& root, const fs::path& candidate) {
    const fs::path relative = candidate.lexically_relative(root);
    if (relative.empty() || relative == ".") {
        return false;
    }
    return *relative.begin() != "..";
}

std::string makeUuid() {
    std::random_device source;
    std::uniform_int_distribution<int> hexDigit(0, 15);
    static const char *pHex = "0123456789abcdef";

    std::string uuid;
    for (int position = 0; position < 36; position++) {
        if (position == 8 || position == 13 || position == 18 || position == 23) {
            uuid += '-';
        } else if (position == 14) {
            uuid += '4';
        } else {
            uuid += pHex[hexDigit(source)];
        }
    }
    return uuid;
}

} // namespace

std::string detectHostColorScheme() {
    // xdg-desktop-portal's org.freedesktop.appearance color-scheme is the
    // cross-desktop standard (0 no preference, 1 dark, 2 light) that GNOME,
    // KDE and the rest implement, so it comes first.
    const std::string portal = runHostCmd({"gdbus", "call", "--session",
            "--dest", "org.freedesktop.portal.Desktop",
            "--object-path", "/org/freedesktop/portal/desktop",
            "--method", "org.freedesktop.portal.Settings.Read",
            "org.freedesktop.appearance", "color-scheme"});

    const size_t at = portal.find("uint32 ");
    if (at != std::string::npos) {
        // Real output is "(<<uint32 1>>,)". Anchored on "uint32 " rather than
        // the first run of digits, which would match the "32" in "uint32".
        const std::string rest = portal.substr(at + 7);
        const std::string value = trimmed(rest.substr(0, rest.find('>')));
        if (value == "1") {
            return "dark";
        }
        if (value == "2") {
            return "light";
        }
        // 0 falls through: some setups answer "no preference" from the portal
        // while the desktop's own setting below is explicit.
    }

    const std::string gsettings =
        runHostCmd({"gsettings", "get", "org.gnome.desktop.interface", "color-scheme"});
    if (!gsettings.empty()) {
        const std::string value = trimmed(gsettings);
        if (value == "prefer-dark") {
            return "dark";
        }
        if (value == "prefer-light") {
            return "light";
        }
    }

    // Last resort for desktops running neither: GTK's own settings file.
    std::string configHome = envOrEmpty("XDG_CONFIG_HOME");
    if (configHome.empty()) {
        configHome = envOrEmpty("HOME") + "/.config";
    }
    for (const std::string gtkDir : {"gtk-4.0", "gtk-3.0"}) {
        std::ifstream settings(configHome + "/" + gtkDir + "/settings.ini");
        if (!settings) {
            continue;
        }
        std::string line;
        while (std::getline(settings, line)) {
            const std::string stripped = trimmed(line);
            if (stripped.rfind("gtk-application-prefer-dark-theme", 0) != 0) {
                continue;
            }
            std::string value = trimmed(stripped.substr(stripped.find('=') + 1));
            for (char& c : value) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            if (value == "1" || value == "true") {
                return "dark";
            }
            if (value == "0" || value == "false") {
                return "light";
            }
        }
    }

    return "";
}

Prefix::Prefix(Proton& protonDist, fs::path base, std::string version)
    : prefixDir(base / "pfx"),
      proton(protonDist),
      baseDir(std::move(base)),
      prefixVersion(std::move(version)),
      versionFile(baseDir / "version"),
      configInfoFile(baseDir / "config_info"),
      trackedFilesFile(baseDir / "tracked_files"),
      creationGuard(prefixDir / "creation_sync_guard"),
      prefixLock(baseDir / "pfx.lock") {
    // The lock lives here, so the directory has to exist before anything
    // tries to take it. On a fresh install nothing has created it yet.
    makeDirs(baseDir);
}

fs::path Prefix::path(const std::string& relative) const {
    return prefixDir / relative;
}

std::string Prefix::readVersion() const {
    std::ifstream in(versionFile);
    if (!in) {
        return "";
    }
    std::string version;
    std::getline(in, version);
    while (!version.empty() && (version.back() == '\r' || version.back() == ' ')) {
        version.pop_back();
    }
    return version;
}

void Prefix::writeVersion() const {
    std::ofstream out(versionFile);
    if (out) {
        out << prefixVersion << "\n";
    }
}

// The registry is the one thing the template hands over for good. Roblox's own
// installers write to it, and so does anything the user changes, so by the time
// an upgrade comes round it holds far more than TuxBlox put there. Replacing it
// with the new template's copy would throw all of that away, which is exactly
// what the old rebuild-everything upgrade did.
static bool holdsAccumulatedState(const std::string& relative) {
    return relative == "system.reg" || relative == "user.reg" || relative == "userdef.reg";
}

void Prefix::removeTrackedFiles(Removal removal) {
    std::ifstream tracked(trackedFilesFile);
    if (!tracked) {
        log("Prefix has no tracked_files??");
        return;
    }

    std::error_code error;
    std::vector<fs::path> directories;
    std::string entry;
    while (std::getline(tracked, entry)) {
        while (!entry.empty() && (entry.back() == '\r' || entry.back() == '\n')) {
            entry.pop_back();
        }
        if (entry.empty()) {
            continue;
        }
        if (removal == Removal::KeepAccumulatedState && holdsAccumulatedState(entry)) {
            continue;
        }
        const fs::path target = prefixDir / entry;
        if (!fileExists(target, false)) {
            continue;
        }
        if (fs::is_directory(fs::symlink_status(target, error))) {
            directories.push_back(target);
        } else {
            fs::remove(target, error);
        }
    }
    tracked.close();

    // Deepest first, so a directory's children are gone before it is tried.
    std::sort(directories.begin(), directories.end(),
              [](const fs::path& a, const fs::path& b) {
                  return a.string().size() > b.string().size();
              });
    for (const fs::path& directory : directories) {
        // Fails harmlessly when the user left something inside.
        fs::remove(directory, error);
    }

    if (removal == Removal::All) {
        fs::remove(trackedFilesFile, error);
        fs::remove(versionFile, error);
    }
}

void Prefix::copyTemplateEntry(const fs::path& src, const fs::path& dst, bool dllCopy) {
    std::error_code error;
    if (isSymlink(src)) {
        fs::path target = fs::read_symlink(src, error);
        if (error) {
            log("Could not read \"" + src.string() + "\": " + error.message());
            return;
        }
        if (fileIsWineBuiltinDll(src) && target.is_relative()) {
            // Point at the real file rather than copying a relative link that
            // would resolve differently from inside the prefix.
            target = (src.parent_path() / target).lexically_normal();
        }
        if (dllCopy) {
            tryCopyFile(src, dst);
            return;
        }
        fs::create_symlink(target, dst, error);
        if (error) {
            log("Could not link \"" + dst.string() + "\" -> \"" + target.string() +
                "\": " + error.message());
        }
        return;
    }
    tryCopyFile(src, dst);
}

void Prefix::copyTemplatePrefix() {
    std::ofstream tracked(trackedFilesFile);
    std::error_code error;

    const fs::path templateDir = proton.defaultPfxDir;
    fs::recursive_directory_iterator walk(templateDir, error);
    if (error) {
        log("Template prefix is missing at \"" + templateDir.string() + "\"");
        return;
    }

    makeDirs(prefixDir);
    for (const fs::directory_entry& entry : walk) {
        // lexically_relative, not fs::relative: the latter resolves symlinks, so a
        // linked builtin would report the path of its target instead of its own.
        const fs::path relative = entry.path().lexically_relative(templateDir);
        if (relative.empty()) {
            continue;
        }
        const fs::path destination = prefixDir / relative;
        if (!isInside(prefixDir, destination)) {
            log("Refusing to write outside the prefix: \"" + destination.string() + "\"");
            continue;
        }

        if (entry.is_directory(error) && !entry.is_symlink(error)) {
            if (!fileExists(destination, true)) {
                makeDirs(destination);
                tracked << relative.string() << "/\n";
            }
            continue;
        }
        if (fileExists(destination, true)) {
            // Ours even though it was left where it was: an upgrade keeps the
            // registry instead of replacing it, and it has to stay on the list
            // so a later removal still takes it.
            if (holdsAccumulatedState(relative.generic_string())) {
                tracked << relative.string() << "\n";
            }
            continue;
        }
        copyTemplateEntry(entry.path(), destination, false);
        tracked << relative.string() << "\n";
    }

    // Set .update-timestamp so Wine does not try to update the prefix itself,
    // which it would whenever wine.inf's timestamp changed.
    std::ofstream stamp(prefixDir / ".update-timestamp");
    if (stamp) {
        stamp << getMtimeStr(proton.wineInf) << "\n";
    }
}

void Prefix::updateBuiltinLibs(const std::string& copyPatterns) {
    const std::vector<std::string> patterns = splitCommas(copyPatterns);

    std::set<std::string> alreadyTracked;
    {
        std::ifstream in(trackedFilesFile);
        std::string entry;
        while (std::getline(in, entry)) {
            alreadyTracked.insert(entry);
        }
    }

    std::ofstream tracked(trackedFilesFile, std::ios::app);
    std::error_code error;
    const fs::path templateDir = proton.defaultPfxDir;

    fs::recursive_directory_iterator walk(templateDir, error);
    if (error) {
        return;
    }

    for (const fs::directory_entry& entry : walk) {
        if (entry.is_directory(error)) {
            continue;
        }
        // lexically_relative, not fs::relative: the latter resolves symlinks, so a
        // linked builtin would report the path of its target instead of its own.
        const fs::path relative = entry.path().lexically_relative(templateDir);
        if (relative.empty()) {
            continue;
        }
        const fs::path destination = prefixDir / relative;
        if (!isInside(prefixDir, destination)) {
            log("Refusing to write outside the prefix: \"" + destination.string() + "\"");
            continue;
        }

        if (!fileIsWineBuiltinDll(entry.path())) {
            continue;
        }
        if (fileIsWineBuiltinDll(destination)) {
            fs::remove(destination, error);
        } else if (fileExists(destination, false)) {
            // The user or an installer replaced this builtin; leave it alone.
            continue;
        } else {
            makeDirs(destination.parent_path());
        }

        const bool dllCopy = matchesAnyPattern(relative.filename().string(), patterns);
        copyTemplateEntry(entry.path(), destination, dllCopy);

        if (alreadyTracked.find(relative.string()) == alreadyTracked.end()) {
            tracked << relative.string() << "\n";
        }
    }
}

void Prefix::createFontSymlinks() {
    const fs::path windowsFonts = prefixDir / "drive_c" / "windows" / "Fonts";
    makeDirs(windowsFonts);

    std::error_code error;
    for (const fs::path& fontsDir : {proton.fontsDir, proton.wineFontsDir}) {
        fs::directory_iterator fonts(fontsDir, error);
        if (error) {
            continue;
        }
        for (const fs::directory_entry& font : fonts) {
            const std::string extension = font.path().extension().string();
            if (extension != ".ttf" && extension != ".ttc") {
                continue;
            }
            const fs::path link = windowsFonts / font.path().filename();
            if (isSymlink(link)) {
                fs::remove(link, error);
            } else if (fileExists(link, false)) {
                continue;
            }
            fs::create_symlink(font.path(), link, error);
        }
    }
}

void Prefix::installGraphicsFiles(Session& session) {
    const GraphicsConfig& graphics = session.graphics;

    std::vector<std::string> dxvkFiles;
    std::vector<std::string> wined3dFiles;

    // Roblox picks its renderer at runtime and does not always land on
    // Vulkan, so whichever Direct3D implementation is selected has to be
    // installed and working even if a given session never touches it.
    if (graphics.useWineD3D) {
        wined3dFiles = {"d3d12", "d3d11", "d3d10", "d3d10core", "d3d10_1", "d3d9"};
    } else {
        // No d3d12 here: vkd3d-proton is not shipped, so the prefix keeps the
        // builtin d3d12 it was created with. Roblox never asks for it anyway.
        dxvkFiles = {"d3d11", "d3d10core", "d3d9"};
    }

    if (graphics.useDxvkDxgi) {
        dxvkFiles.push_back("dxgi");
    } else {
        wined3dFiles.push_back("dxgi");
    }

    std::ofstream tracked(trackedFilesFile, std::ios::app);

    CopyOptions options;
    options.prefix = prefixDir.string();
    options.pTrackFile = &tracked;
    options.linkDebug = true;

    for (const std::string& name : wined3dFiles) {
        copyPath(proton.defaultPfxDir / "drive_c/windows/system32" / (name + ".dll"),
                 "drive_c/windows/system32", options);
        copyPath(proton.defaultPfxDir / "drive_c/windows/syswow64" / (name + ".dll"),
                 "drive_c/windows/syswow64", options);
    }

    for (const std::string& name : dxvkFiles) {
        copyPath(proton.archPeDir("wine/dxvk", false) / (name + ".dll"),
                 "drive_c/windows/system32", options);
        copyPath(proton.archPeDir("wine/dxvk", true) / (name + ".dll"),
                 "drive_c/windows/syswow64", options);
        session.dllOverrides[name] = "n";
    }

    if (graphics.useNvapi) {
        copyPath(proton.archPeDir("wine/nvapi", false) / "nvapi64.dll",
                 "drive_c/windows/system32", options);
        copyPath(proton.archPeDir("wine/nvapi", true) / "nvapi.dll",
                 "drive_c/windows/syswow64", options);
        session.dllOverrides["nvapi64"] = "n";
        session.dllOverrides["nvapi"] = "n";
    } else {
        std::error_code error;
        fs::remove(prefixDir / "drive_c/windows/system32/nvapi64.dll", error);
        fs::remove(prefixDir / "drive_c/windows/syswow64/nvapi.dll", error);
    }
}

void Prefix::migrateUserPaths() {
    // Wine's own compatibility links: apps that still use the Windows XP
    // folder names find the modern ones through these.
    const std::array<std::array<std::string, 2>, 3> links = {{
        {"drive_c/users/user/Local Settings/Application Data", "../AppData/Local"},
        {"drive_c/users/user/Application Data", "./AppData/Roaming"},
        {"drive_c/users/user/My Documents", "./Documents"}
    }};

    std::error_code error;
    for (const auto& [relative, target] : links) {
        const fs::path link = prefixDir / relative;
        if (fileExists(link, false)) {
            if (!isSymlink(link)) {
                continue;
            }
            if (fs::read_symlink(link, error) == fs::path(target)) {
                continue;
            }
            fs::remove(link, error);
        }
        makeDirs(link.parent_path());
        fs::create_symlink(target, link, error);
    }
}

// PlayStation pads Wine routes to its own hidraw implementation. Sony's vendor
// id with the pads Wine names in is_dualshock4_gamepad/is_dualsense_gamepad.
const char *const kPlayStationPads[] = {
    "054C/05C4", // DualShock 4 [CUH-ZCT1x]
    "054C/09CC", // DualShock 4 [CUH-ZCT2x]
    "054C/0BA0", // DualShock 4 wireless adaptor
    "054C/0CE6", // DualSense
    "054C/0DF2", // DualSense Edge
};

void Prefix::syncHaptics() {
    // Roblox asks for vibration through XInput -- its binaries carry
    // HapticService, SetMotor and XInputSetState, and no Windows.Gaming.Input
    // force-feedback interface at all. A PlayStation pad is not an XInput
    // device, so unless something presents it as one the motors cannot be
    // reached, on Windows either.
    //
    // Wine hands these pads to its own hidraw implementation, which describes
    // the pad more fully but has no vibration. Turning hidraw off for just
    // these devices hands them to SDL instead, which winexinput.sys then wraps
    // as an XInput device, and the motors answer. The cost is one button off
    // the pad, which is why the launcher offers this as a setting.
    //
    // Per device rather than the PROTON_DISABLE_HIDRAW variable that does the
    // same thing: that one is global, and winebus reads it out of the Windows
    // environment block, which TuxBlox filters precisely so Roblox cannot read
    // the host's environment. Passing it would put the string
    // "PROTON_DISABLE_HIDRAW" in front of Roblox.
    const bool enabled = envOrEmpty("TUXBLOX_HAPTICS") != "0";
    const std::string hidraw = enabled ? "dword:00000000" : "dword:00000001";

    const fs::path systemReg = prefixDir / "system.reg";
    bool changed = false;
    for (const char *pPad : kPlayStationPads) {
        changed |= setRegKeyValues(systemReg,
                std::string("System\\\\ControlSet001\\\\Services\\\\winebus\\\\Devices\\\\") + pPad,
                {{"Hidraw", hidraw}});
    }

    if (changed) {
        log(enabled ? "Enabled controller vibration" : "Disabled controller vibration");
    }
}

void Prefix::syncHostTheme() {
    const std::string scheme = detectHostColorScheme();
    if (scheme.empty()) {
        return;
    }

    nlohmann::json colors;
    try {
        colors = nlohmann::json::parse(tuxblox_data::find("syscolors.json"));
    } catch (const nlohmann::json::exception& failure) {
        log(std::string("Could not read system colours: ") + failure.what());
        return;
    }
    if (!colors.contains(scheme)) {
        return;
    }

    const fs::path userReg = prefixDir / "user.reg";
    const std::string lightTheme = (scheme == "light") ? "dword:00000001" : "dword:00000000";

    // Read by uxtheme's ShouldAppsUseDarkMode()/ShouldSystemUseDarkMode() and
    // by windows.ui's UISettings, which is how Studio's Qt UI and WebView2
    // pick a colour scheme.
    bool changed = setRegKeyValues(userReg,
            "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Themes\\\\Personalize",
            {{"AppsUseLightTheme", lightTheme}, {"SystemUsesLightTheme", lightTheme}});

    std::map<std::string, std::string> colorValues;
    for (const auto& [name, value] : colors[scheme].items()) {
        colorValues[name] = "\"" + value.get<std::string>() + "\"";
    }

    // GetSysColor(), meaning everything Wine draws itself.
    changed |= setRegKeyValues(userReg, "Control Panel\\\\Colors", colorValues);

    // uxtheme keeps a second copy under ThemeManager as the "colours from
    // before a theme was applied" backup, and restores it over the live ones
    // whenever theming is switched off at runtime. Keeping the backup in step
    // stops that restore repainting the prefix in Wine's old classic beige.
    changed |= setRegKeyValues(userReg,
            "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\ThemeManager\\\\Control Panel\\\\Colors",
            colorValues);

    // The colours above only reach controls Wine draws classically: the
    // msstyles theme the template prefix ships paints scrollbars, buttons and
    // tabs from baked-in light bitmaps, and Wine has no dark theme to switch
    // to. So dark mode turns theming off and light mode turns it back on.
    changed |= setRegKeyValues(userReg,
            "Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\ThemeManager",
            {{"ThemeActive", scheme == "light" ? "\"1\"" : "\"0\""}});

    if (changed) {
        log("Synced prefix to host " + scheme + " theme");
    }
}

void Prefix::setup(Session& session) {
    FileLock::Guard held(prefixLock);

    const std::string oldVersion = readVersion();

    // An upgrade replaces what TuxBlox itself laid down and nothing else. The
    // tracked list says exactly which files those are, so a stale one cannot
    // survive, while installed Roblox versions, the registry and anything else
    // the user or Roblox added are left where they are. Rebuilding the whole
    // prefix instead, as this used to, threw away 225MB of Windows files and
    // every registry key on every release.
    if (!oldVersion.empty() && oldVersion != prefixVersion) {
        log("Prefix was built by TuxBlox " + oldVersion + ", updating it for " + prefixVersion);
        removeTrackedFiles(Removal::KeepAccumulatedState);
        copyTemplatePrefix();
    }

    if (!fileExists(creationGuard, false)) {
        makeDirs(prefixDir / "drive_c");
        setDirCasefoldBit(prefixDir / "drive_c");

        copyTemplatePrefix();

        replaceRegValue(prefixDir / "system.reg",
                        "Software\\\\Microsoft\\\\Cryptography", "MachineGuid",
                        "\"" + makeUuid() + "\"");
        ::sync();

        std::ofstream guard(creationGuard);
        guard.close();
        ::sync();
    }

    migrateUserPaths();
    syncHostTheme();
    syncHaptics();

    std::error_code error;
    const fs::path driveC = prefixDir / "dosdevices" / "c:";
    if (!fileExists(driveC, false)) {
        makeDirs(prefixDir / "dosdevices");
        fs::create_symlink("../drive_c", driveC, error);
    }

    // No Z: drive. Mapping the whole host root as a drive letter is one of the
    // most common ways to detect Wine, and Roblox lives entirely under C:, so
    // nothing legitimately needs it. Absolute unix paths still resolve through
    // ntdll's drive-independent fallback.

    const std::string dllCopyPatterns = []() {
        const std::string override = envOrEmpty("TUXBLOX_DLL_COPY");
        return override.empty() ? DefaultDllCopyPatterns : override;
    }();

    // Anything here changing means the builtin libraries have to be laid down
    // again, so it is recorded and compared on the next launch.
    std::string configInfo = prefixVersion;
    configInfo += "\n" + proton.fontsDir.string();
    configInfo += "\n" + proton.libDir.string();
    configInfo += "\n" + proton.defaultPfxDir.string();
    configInfo += "\n" + getMtimeStr(proton.defaultPfxDir / "system.reg");
    configInfo += "\n" + std::string(session.graphics.useWineD3D ? "1" : "0");
    configInfo += "\n" + std::string(session.graphics.useDxvkDxgi ? "1" : "0");
    configInfo += "\n" + std::string(session.graphics.useNvapi ? "1" : "0");
    configInfo += "\n" + dllCopyPatterns;

    std::string oldConfigInfo;
    {
        std::ifstream in(configInfoFile);
        if (in) {
            std::stringstream buffer;
            buffer << in.rdbuf();
            oldConfigInfo = buffer.str();
        }
    }

    if (oldVersion != prefixVersion || oldConfigInfo != configInfo) {
        updateBuiltinLibs(dllCopyPatterns);
        std::ofstream out(configInfoFile);
        if (out) {
            out << configInfo;
        }
    }

    writeVersion();
    createFontSymlinks();
    installGraphicsFiles(session);
}

} // namespace tuxblox
