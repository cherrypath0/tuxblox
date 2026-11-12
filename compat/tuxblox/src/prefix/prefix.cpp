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
#include "prefix/webview_mode.h"
#include "support/account_name.h"
#include "support/host_folders.h"
#include "FlagManager.h"
#include "shared_paths/windows_paths.h"
#include "embedded_data.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <map>
#include <random>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <vector>

#include <cerrno>
#include <fcntl.h>
#include <fnmatch.h>
#include <sys/stat.h>
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
    // Roblox's anti-tamper loads the official loader.
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

// Lays the chosen Direct3D libraries into the drive. Which ones those are is graphicsFiles()' answer, and asking Roblox to actually use them is Session::applyDllOverrides()' job -- that part is not drive state and has to happen on every launch, including the ones that never come through here.
void Prefix::installGraphicsFiles(const GraphicsConfig& graphics) {
    const GraphicsFiles files = graphicsFiles(graphics);

    std::ofstream tracked(trackedFilesFile, std::ios::app);

    CopyOptions options;
    options.prefix = prefixDir.string();
    options.pTrackFile = &tracked;
    options.linkDebug = true;

    for (const std::string& name : files.wined3d) {
        copyPath(proton.defaultPfxDir / "drive_c/windows/system32" / (name + ".dll"),
                 "drive_c/windows/system32", options);
        copyPath(proton.defaultPfxDir / "drive_c/windows/syswow64" / (name + ".dll"),
                 "drive_c/windows/syswow64", options);
    }

    for (const std::string& name : files.dxvk) {
        copyPath(proton.archPeDir("wine/dxvk", false) / (name + ".dll"),
                 "drive_c/windows/system32", options);
        copyPath(proton.archPeDir("wine/dxvk", true) / (name + ".dll"),
                 "drive_c/windows/syswow64", options);
    }

    if (files.nvapi) {
        copyPath(proton.archPeDir("wine/nvapi", false) / "nvapi64.dll",
                 "drive_c/windows/system32", options);
        copyPath(proton.archPeDir("wine/nvapi", true) / "nvapi.dll",
                 "drive_c/windows/syswow64", options);
    } else {
        std::error_code error;
        fs::remove(prefixDir / "drive_c/windows/system32/nvapi64.dll", error);
        fs::remove(prefixDir / "drive_c/windows/syswow64/nvapi.dll", error);
    }
}

// Windows programs write their scratch files into AppData\\Local\\Temp, and every
// one of them stays in the drive until something clears it out. Pointing it at a
// folder of our own under the computer's temporary directory hands that job to
// the system, which empties it on its own schedule.
//
// The folder is per user and per drive: a name everyone shares in a place
// everyone can write is how one account ends up writing through another's link,
// and two drives running at once must not share one scratch folder. Anything
// already there that is not ours is left alone rather than replaced.
void Prefix::linkTempDir() {
    struct stat info {};
    if (::stat("/tmp", &info) != 0 || !S_ISDIR(info.st_mode)) {
        return; // no temporary directory on this computer; leave the drive's own
    }

    const fs::path base = "/tmp/tuxblox-" + std::to_string(::getuid());
    if (::mkdir(base.c_str(), 0700) != 0 && errno != EEXIST) {
        return;
    }
    if (::lstat(base.c_str(), &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != ::getuid()) {
        log("Not using " + base.string() + " for temporary files: it is not ours");
        return;
    }

    struct stat prefixInfo {};
    if (::stat(prefixDir.c_str(), &prefixInfo) != 0) {
        return;
    }
    char leafName[64];
    std::snprintf(leafName, sizeof(leafName), "%llx-%llx",
                  static_cast<unsigned long long>(prefixInfo.st_dev),
                  static_cast<unsigned long long>(prefixInfo.st_ino));
    const fs::path leaf = base / leafName;
    if (::mkdir(leaf.c_str(), 0700) != 0 && errno != EEXIST) {
        return;
    }
    if (::lstat(leaf.c_str(), &info) != 0 || !S_ISDIR(info.st_mode) || info.st_uid != ::getuid()) {
        return;
    }

    std::error_code error;
    const fs::path temp = prefixDir / "drive_c/users" / accountFolder() / "AppData/Local/Temp";
    if (isSymlink(temp)) {
        if (fs::read_symlink(temp, error) == leaf) {
            return; // already points at it
        }
        fs::remove(temp, error);
    } else if (fileExists(temp, true)) {
        if (!fs::is_empty(temp, error) || error) {
            return; // someone's files are in there; not ours to move
        }
        fs::remove(temp, error);
    }

    makeDirs(temp.parent_path());
    fs::create_directory_symlink(leaf, temp, error);
    if (error) {
        log("Could not point the drive's temporary folder at " + leaf.string());
    }
}

// Points the drive's Desktop, Documents, Downloads, Music, Pictures and Videos at the matching folders in the user's home, so Roblox can open and save there directly instead of only seeing files handed to it one at a time.
void Prefix::linkHostUserFolders() {
    const std::string home = envOrEmpty("HOME");
    if (home.empty()) {
        return;
    }

    const fs::path account = prefixDir / "drive_c/users" / accountFolder();

    // Before Documents starts pointing at the real one, or this would be carried into it.
    if (!removeLegacyBridgeRoot(account / "Documents")) {
        log("Could not remove the old \"TuxBlox Files\" folder; leaving it where it is.");
    }

    for (const HostFolder& folder : hostFolderMappings()) {
        const std::string hostPath = hostFolderPath(folder.xdgKey, folder.windowsName, home);
        if (hostPath.empty()) {
            continue;
        }
        switch (linkHostFolder(account / folder.windowsName, hostPath)) {
        case HostFolderLink::Kept:
            log("Keeping the drive's own \"" + folder.windowsName +
                "\" folder: something in it could not be moved to " + hostPath);
            break;
        case HostFolderLink::Linked:
        case HostFolderLink::AlreadyLinked:
        case HostFolderLink::NoHostFolder:
            break;
        }
    }
}

// Where Roblox's own data is kept. Normally a folder in the install folder, so that
// rebuilding the virtual drive does not take a few gigabytes of installed Roblox down
// with it. With the experimental Linux integration turned on it is the folder the
// desktop names for application data instead, with the folders in robloxDataSplits()
// carved out of it into the state and cache folders beside it.
fs::path Prefix::robloxDataDir(const fs::path& installRoot) const {
    if (!flagBool("tbxFlagEnableExperimentalLinuxIntegration")) {
        return installRoot / "roblox";
    }
    const std::string data = xdgBaseDir("XDG_DATA_HOME", envOrEmpty("HOME"));
    if (data.empty()) {
        return installRoot / "roblox"; // no home folder, so nowhere to follow the desktop into
    }
    return fs::path(data) / "roblox";
}

// Where the data is now, which is not always where it belongs: it moves when the
// experimental Linux integration is switched either way, and a drive built before 3.10.0
// kept it inside the drive. Empty when there is none yet.
fs::path Prefix::currentRobloxDataDir(const fs::path& robloxData,
                                      const fs::path& installRoot) const {
    std::error_code error;
    const fs::path candidates[] = {robloxData, installRoot / "roblox"};
    for (const fs::path& candidate : candidates) {
        if (isSymlink(candidate)) {
            const fs::path target = fs::read_symlink(candidate, error);
            if (!error && target.is_absolute() && fs::is_directory(target, error)) return target;
        } else if (fs::is_directory(candidate, error)) {
            return candidate;
        }
    }
    return {};
}

// Moves a folder to where it belongs and leaves a link behind in its place. One rename
// wherever that is possible, which is instant however large the folder is and keeps every
// folder in it, the empty ones included; linkHostFolder() moves entry by entry otherwise,
// which is needed only when there is already something on both sides to merge. False when
// something had to stay behind, which the caller reports.
bool Prefix::moveFolderAndLink(const fs::path& from, const fs::path& to) {
    std::error_code error;
    const bool spare = !fileExists(to, false) ||
                       (fs::is_directory(to, error) && fs::is_empty(to, error));
    if (spare && !isSymlink(from) && fs::is_directory(from, error)) {
        makeDirs(to.parent_path());
        fs::remove(to, error);
        fs::rename(from, to, error);
    }
    makeDirs(to);
    // A rename above leaves nothing at `from`, so this only creates the link; without one it moves what is there across first.
    return linkHostFolder(from, to) != HostFolderLink::Kept;
}

// Points the carved-out folders at the desktop's state and cache folders, or puts them
// back when the integration is switched off again -- which has to be done either way, or
// a folder would go on being written somewhere TuxBlox no longer believes it is.
void Prefix::splitRobloxDataFolders(const fs::path& dataDir) {
    const bool wanted = flagBool("tbxFlagEnableExperimentalLinuxIntegration");
    const std::string home = envOrEmpty("HOME");

    for (const RobloxDataSplit& split : robloxDataSplits()) {
        const std::string base = xdgBaseDir(split.xdgKey, home);
        if (base.empty()) continue;
        const fs::path inData = dataDir / split.name;
        const fs::path outside = fs::path(base) / "roblox" / split.name;
        std::error_code error;

        if (wanted) {
            if (!moveFolderAndLink(inData, outside)) {
                log("Keeping Roblox's \"" + split.name + "\" folder where it is: something in "
                    "it could not be moved to " + outside.string());
            }
            continue;
        }

        // Switched off: only a link this made is undone, and only back into the folder it came out of.
        if (!isSymlink(inData) || fs::read_symlink(inData, error) != outside || error) continue;
        fs::remove(inData, error);
        if (!moveFolderAndLink(outside, inData)) {
            log("Leaving Roblox's \"" + split.name + "\" folder in " + outside.string() +
                ": something in it could not be moved back");
            continue;
        }
        fs::remove(outside, error); // the link left in its place leads back into the drive's own data, and nothing should follow it there
        fs::remove(outside.parent_path(), error); // fails harmlessly while a folder that could not be moved back is still in it
    }
}

void Prefix::linkRobloxData() {
    // Roblox keeps its installed versions, settings and logs here. It is always
    // present, even in a brand-new prefix, and Roblox fills it in on first launch.
    const fs::path robloxData =
        prefixDir / "drive_c/users" / accountFolder() / "AppData/Local/Roblox";

    std::error_code error;
    const fs::path installRoot = fs::weakly_canonical(baseDir.parent_path(), error);
    if (error || !installRoot.is_absolute()) {
        makeDirs(robloxData);
        return; // without a dependable path outside the drive the data stays inside it
    }
    const fs::path inInstall = installRoot / "roblox";
    const fs::path dataDir = robloxDataDir(installRoot);
    const fs::path current = currentRobloxDataDir(robloxData, installRoot);

    if (!current.empty() && current != dataDir) {
        // Put the folders back before the data moves, so they travel with it rather than
        // being left behind pointing into the place it moved out of.
        splitRobloxDataFolders(current);
        // The data belongs at this path now, and what is there is TuxBlox's own shortcut to where it used to be.
        if (dataDir == inInstall && isSymlink(inInstall)) fs::remove(inInstall, error);

        if (!moveFolderAndLink(current, dataDir)) {
            log("Keeping Roblox's data in " + current.string() +
                ": something in it could not be moved to " + dataDir.string());
        } else if (current != inInstall) {
            fs::remove(current, error); // a link back is wanted in the install folder and nowhere else
        }
    }

    makeDirs(dataDir);
    makeDirs(robloxData.parent_path());

    switch (linkHostFolder(robloxData, dataDir)) {
    case HostFolderLink::Kept:
        log("Keeping Roblox's data inside the virtual drive: something in it could not "
            "be moved to " + dataDir.string());
        break;
    case HostFolderLink::NoHostFolder:
        makeDirs(robloxData); // nowhere outside the drive to keep it, so it stays inside
        break;
    case HostFolderLink::Linked:
    case HostFolderLink::AlreadyLinked:
        break;
    }

    // Nothing of TuxBlox's own is left in the install folder when the data is kept in the
    // desktop's folders instead: every program finds it by following the folder inside the
    // drive, which robloxDataDir() is the one rule for, so a second way to the same place
    // would only be another thing able to go stale.
    if (dataDir != inInstall && isSymlink(inInstall)) fs::remove(inInstall, error);

    splitRobloxDataFolders(dataDir);
}
// The account folder this drive uses. Whatever Wine created is the truth, and when the drive holds more than one the host's own name settles it -- that is the folder %USERPROFILE% names, so it is where Windows programs will actually be writing.
std::string Prefix::accountFolder() const {
    const std::string wanted = accountName();
    const std::string found = prefixAccountName(prefixDir, wanted);
    return found.empty() ? wanted : found;
}

// Older drives were built with a folder called "user". Renaming it keeps the installed Roblox, the login and the settings that live inside it.
void Prefix::migrateAccountFolder() {
    const std::string wanted = accountName();
    switch (tuxblox::migrateAccountFolder(prefixDir, wanted)) {
    case AccountFolderMigration::Renamed:
        log("Renamed the virtual drive's account folder from \"user\" to \"" + wanted + "\".");
        break;
    case AccountFolderMigration::Failed:
        log("Could not rename the account folder to \"" + wanted + "\", keeping \"user\".");
        break;
    case AccountFolderMigration::RegistryIncomplete:
        log("Renamed the virtual drive's account folder to \"" + wanted +
            "\", but could not update every setting that records the old path.");
        break;
    case AccountFolderMigration::NotNeeded:
        break;
    }
}

// Microsoft mode deletes our runtime out of the drive, and the template prefix is only copied over again on an upgrade, so switching back has to put the files there itself.
void Prefix::restoreWebViewRuntime() {
    const fs::path runtime = fs::path(WebView2ApplicationDir) / kWebView2RuntimeVersion;
    for (const std::string& relative : WebView2RuntimeDlls) {
        const fs::path source = proton.defaultPfxDir / runtime / relative;
        const fs::path destination = prefixDir / runtime / relative;
        if (fileExists(destination, false) || !fileExists(source, true)) {
            continue;
        }
        makeDirs(destination.parent_path());
        copyTemplateEntry(source, destination, false);
    }
}

void Prefix::syncWebViewVersionKeys(bool microsoft) {
    if (syncWebViewValues(prefixDir, microsoft)) {
        log("The virtual drive already has a Microsoft browser runtime; leaving it alone, so programs that go "
            "looking for one will use Microsoft's rather than TuxBlox's.");
    }
}

// Recorded in the drive because the parts of the runtime that act on it are Windows-side, where TuxBlox's own environment variables are deliberately not visible.
void Prefix::recordWebViewMode() {
    const bool microsoft = useMicrosoftWebView();
    setRegKeyValues(prefixDir / "user.reg", "Software\\\\TuxBlox",
                    {{"WebView2", "\"" + webViewModeName() + "\""}});

    if (!microsoft) {
        restoreWebViewRuntime();
        syncWebViewVersionKeys(false);
        return;
    }

    // Dropping the entries that point at a runtime we did not put there would break somebody's real install, which is the one Microsoft mode is meant to hand over to.
    if (!removeFakeWebViewRuntime(prefixDir)) {
        log("Leaving the WebView2 runtime in the virtual drive alone: it is not the one TuxBlox put there.");
        return;
    }
    syncWebViewVersionKeys(true);
}

void Prefix::migrateUserPaths() {
    // Wine's own compatibility links: apps that still use the Windows XP
    // folder names find the modern ones through these.
    const std::string account = "drive_c/users/" + accountFolder() + "/";
    const std::array<std::array<std::string, 2>, 3> links = {{
        {account + "Local Settings/Application Data", "../AppData/Local"},
        {account + "Application Data", "./AppData/Roaming"},
        {account + "My Documents", "./Documents"}
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

void Prefix::canonicaliseRegistryPaths() {
    // Roblox builds its paths by pasting onto %LOCALAPPDATA%, so the drive is
    // only as capitalised as the strings the registry hands out. The folders
    // themselves stay lower case: this drive cannot open a capitalised name.
    bool changed = false;
    for (const char* file : {"user.reg", "system.reg", "userdef.reg"}) {
        changed |= canonicaliseRegFile(prefixDir / file);
    }

    if (changed) {
        log("Corrected the capitalisation of the virtual drive's Windows folders");
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

// The desktop's subpixel layout, as one of rgb, bgr, none or "" when the
// desktop does not say. This is the same X resource the graphics driver reads
// when it decides how to rasterise a glyph, so reading it here is what keeps
// the drive's answer and the rasteriser's behaviour from disagreeing.
std::string detectHostSubpixel() {
    const std::string resources = runHostCmd({"xrdb", "-query"});
    std::istringstream lines(resources);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("Xft.rgba:", 0) == 0) {
            return trimmed(line.substr(line.find(':') + 1));
        }
    }
    return "";
}

// An unscaled desktop, in the units Windows counts interface scale in.
constexpr int kUnscaledDpi = 96;

// The desktop's interface scale as a DPI. 0 means the desktop never said, which
// is not the same as 96: it leaves the drive's own default alone.
int detectHostDpi() {
    // Xft.dpi is what an X11 desktop writes when the user picks a scale, and it
    // arrives in the same resource dump the subpixel layout is read from.
    const std::string resources = runHostCmd({"xrdb", "-query"});
    std::istringstream lines(resources);
    std::string line;
    while (std::getline(lines, line)) {
        if (line.rfind("Xft.dpi:", 0) == 0) {
            const int dpi = std::atoi(trimmed(line.substr(line.find(':') + 1)).c_str());
            if (dpi > 0) {
                return dpi;
            }
        }
    }

    // A Wayland session leaves the X resources alone, so ask GNOME itself. It
    // keeps a multiple of the unscaled size rather than a DPI.
    const std::string factor = trimmed(runHostCmd({"gsettings", "get",
            "org.gnome.desktop.interface", "text-scaling-factor"}));
    if (!factor.empty()) {
        const double scale = std::atof(factor.c_str());
        if (scale > 0.0) {
            return static_cast<int>(kUnscaledDpi * scale + 0.5);
        }
    }

    // A session that scales by whole numbers says so here and nowhere else.
    const char* gdkScale = std::getenv("GDK_SCALE");
    if (gdkScale != nullptr) {
        const int scale = std::atoi(gdkScale);
        if (scale > 0) {
            return kUnscaledDpi * scale;
        }
    }
    return 0;
}

// A LOGFONTW for the registry: five LONGs, eight bytes of flags, then the
// face name as 32 UTF-16 characters. Wine reads these back for the fonts it
// reports through SystemParametersInfo.
std::string logFontValue(const std::string& family, int points, int dpi) {
    std::array<unsigned char, 92> blob{};

    // Negative asks for a character height rather than a cell height, which is
    // what a point size means. The height is in the drive's own pixels, so it
    // has to follow the LogPixels the drive reports or the text comes out the
    // wrong size against everything sized from it.
    const int height = -((points * dpi + 36) / 72);
    const unsigned int cellHeight = static_cast<unsigned int>(height);
    const unsigned int weight = 400;
    for (int byte = 0; byte < 4; byte++) {
        blob[byte] = static_cast<unsigned char>((cellHeight >> (byte * 8)) & 0xff);
        blob[16 + byte] = static_cast<unsigned char>((weight >> (byte * 8)) & 0xff);
    }
    blob[23] = 1; // DEFAULT_CHARSET

    for (size_t index = 0; index < family.size() && index < 31; index++) {
        blob[28 + index * 2] = static_cast<unsigned char>(family[index]);
    }

    std::string hex = "hex:";
    char pair[4];
    for (size_t index = 0; index < blob.size(); index++) {
        std::snprintf(pair, sizeof(pair), "%02x", blob[index]);
        hex += pair;
        if (index + 1 < blob.size()) {
            hex += ",";
        }
    }
    return hex;
}

void Prefix::syncHostDpi() {
    const int detected = detectHostDpi();

    // An unscaled desktop, or one that never said: the drive's own 96 is already
    // right, and writing it would only leave a value behind to go stale.
    if (detected <= kUnscaledDpi) {
        return;
    }

    // Studio's Qt reads this once at startup and sizes its whole interface from
    // it, so a drive left at 96 draws it a quarter of the area the same desktop
    // gives everything else on a 4K screen. Clamped because a desktop can report
    // a scale nothing would fit on.
    const int dpi = detected > 480 ? 480 : detected;
    char value[24];
    std::snprintf(value, sizeof(value), "dword:%08x", static_cast<unsigned int>(dpi));

    const bool changed = setRegKeyValues(prefixDir / "user.reg",
            "Control Panel\\\\Desktop", {{"LogPixels", value}});

    if (changed) {
        log("Synced prefix to host interface scale, " + std::to_string(dpi) + " DPI");
    }
}

// Studio's own interface font, which it ships and loads itself, so it draws here exactly
// as it does on Windows. Windows reports Segoe UI for these, and naming that instead is
// close but visibly not the same face -- Studio's menus are the one place it shows.
constexpr const char* kStudioUiFont = "Source Sans Pro";
constexpr int kStudioUiPoints = 9;

void Prefix::syncHostFont() {
    const int detected = detectHostDpi();
    const std::string font = logFontValue(kStudioUiFont, kStudioUiPoints,
            detected > 0 ? detected : kUnscaledDpi);
    const fs::path userReg = prefixDir / "user.reg";

    // All six, because Studio takes its menus from one, its docked panel titles from
    // another and its dialogs from a third, and on Windows they are the same font.
    bool changed = setRegKeyValues(userReg,
            "Control Panel\\\\Desktop\\\\WindowMetrics",
            {{"MessageFont", font}, {"MenuFont", font}, {"StatusFont", font},
             {"CaptionFont", font}, {"SmCaptionFont", font}, {"IconFont", font}});

    // A drive built before this sent that name to Times New Roman, so the interface
    // came out in a serif. New drives get this from the layer's own setup script.
    changed |= setRegKeyValues(userReg, "Software\\\\TuxBlox\\\\Fonts\\\\Replacements",
            {{"Segoe UI", "\"Selawik\""}, {"Segoe UI Semibold", "\"Selawik Semibold\""}});

    if (changed) {
        log(std::string("Synced prefix to Studio's interface font, ") + kStudioUiFont);
    }
}

void Prefix::syncFontSmoothing() {
    const std::string layout = detectHostSubpixel();

    // Windows reports smoothing type 2 on every machine, and Studio's Qt reads
    // it once at startup to pick between its two text paths. Answering 1 sent
    // it down the grey path, where it flattens the colour-striped glyphs the
    // rasteriser actually returns into one grey channel -- which is what made
    // its text heavier and softer than the same Qt on Windows. A desktop with
    // no stripe order, or a vertical one Windows cannot describe, keeps the
    // grey path, because there the flattening is correct.
    const bool striped = layout.empty() || layout == "rgb" || layout == "bgr";
    const std::string type = striped ? "dword:00000002" : "dword:00000001";
    const std::string orientation = (layout == "bgr") ? "dword:00000000" : "dword:00000001";

    const bool changed = setRegKeyValues(prefixDir / "user.reg",
            "Control Panel\\\\Desktop",
            {{"FontSmoothingType", type}, {"FontSmoothingOrientation", orientation}});

    if (changed) {
        log("Synced prefix to host text smoothing " + (layout.empty() ? "rgb" : layout));
    }
}

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
    // The palette is applied once and then left alone, so colours set by hand
    // in the drive's registry are never written over on a later launch.
    if (!getRegValue(prefixDir / "user.reg", "Software\\\\TuxBlox\\\\Theme", "Applied").empty()) {
        return;
    }

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

    setRegKeyValues(userReg, "Software\\\\TuxBlox\\\\Theme", {{"Applied", "\"" + scheme + "\""}});

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

    migrateAccountFolder();
    migrateUserPaths();
    canonicaliseRegistryPaths();
    linkHostUserFolders();
    linkRobloxData();
    linkTempDir();
    syncHostTheme();
    syncHostDpi();
    syncHostFont();
    syncFontSmoothing();
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

    // Last, because copyTemplatePrefix() and updateBuiltinLibs() both lay the WebView2 runtime back down from the template whenever the drive is missing it.
    recordWebViewMode();

    writeVersion();
    createFontSymlinks();
    installGraphicsFiles(session.graphics);
}

} // namespace tuxblox
