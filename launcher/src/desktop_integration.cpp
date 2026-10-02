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

#include "desktop_integration.h"
#include "tuxblox_logo_png.h" // generated at build time: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include "container_env.h"
#include "wine_shortcut_export.h"
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::vector<std::string> xdgDirList(const char* pVariable, const std::string& fallback) {
    const char* value = std::getenv(pVariable);
    const std::string dirs = (value != nullptr && value[0] != '\0') ? value : fallback;

    std::vector<std::string> out;
    size_t at = 0;
    while (at <= dirs.size()) {
        const size_t colon = dirs.find(':', at);
        const std::string dir = dirs.substr(at, colon == std::string::npos ? std::string::npos : colon - at);
        if (!dir.empty()) out.push_back(dir);
        if (colon == std::string::npos) break;
        at = colon + 1;
    }
    return out;
}

std::string xdgHomeDir(const char* pVariable, const char* pDefaultSuffix) {
    const char* value = std::getenv(pVariable);
    if (value != nullptr && value[0] != '\0') return value;
    const char* home = std::getenv("HOME");
    if (home == nullptr || home[0] == '\0') return "";
    return std::string(home) + pDefaultSuffix;
}

// Every mimeapps.list that can hold a default, in the order the desktop reads them.
std::vector<std::string> mimeappsCandidatePaths() {
    std::vector<std::string> paths;

    // A desktop can keep its own list, and that one wins over the shared one.
    std::vector<std::string> prefixes;
    for (const std::string& desktop : xdgDirList("XDG_CURRENT_DESKTOP", "")) {
        std::string lowered = desktop;
        for (char& c : lowered) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        prefixes.push_back(lowered + "-");
    }
    prefixes.push_back("");

    const std::string configHome = xdgHomeDir("XDG_CONFIG_HOME", "/.config");
    const std::string dataHome = xdgHomeDir("XDG_DATA_HOME", "/.local/share");
    for (const std::string& prefix : prefixes) {
        if (!configHome.empty()) paths.push_back(configHome + "/" + prefix + "mimeapps.list");
        // Where these used to live; still read, since an install older than the move keeps its choices here.
        if (!dataHome.empty()) paths.push_back(dataHome + "/applications/" + prefix + "mimeapps.list");
    }
    for (const std::string& prefix : prefixes) {
        for (const std::string& dir : xdgDirList("XDG_CONFIG_DIRS", "/etc/xdg")) {
            paths.push_back(dir + "/" + prefix + "mimeapps.list");
        }
        for (const std::string& dir : xdgDirList("XDG_DATA_DIRS", "/usr/local/share:/usr/share")) {
            paths.push_back(dir + "/applications/" + prefix + "mimeapps.list");
        }
    }
    return paths;
}

std::string readWholeFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return "";
    std::ostringstream out;
    out << file.rdbuf();
    return out.str();
}

// Whether a desktop id names an entry that is actually installed, searched the way the desktop itself
// searches: the user's own applications directory first, then every directory in $XDG_DATA_DIRS.
bool desktopEntryInstalled(const std::string& desktopId) {
    if (desktopId.empty()) return false;

    std::vector<std::string> roots;
    const char* dataHome = std::getenv("XDG_DATA_HOME");
    if (dataHome != nullptr && dataHome[0] != '\0') {
        roots.push_back(dataHome);
    } else if (const char* home = std::getenv("HOME"); home != nullptr && home[0] != '\0') {
        roots.push_back(std::string(home) + "/.local/share");
    }

    const char* dataDirs = std::getenv("XDG_DATA_DIRS");
    const std::string dirs = (dataDirs != nullptr && dataDirs[0] != '\0')
                                  ? dataDirs
                                  : "/usr/local/share:/usr/share";
    size_t at = 0;
    while (at <= dirs.size()) {
        const size_t colon = dirs.find(':', at);
        const std::string dir = dirs.substr(at, colon == std::string::npos ? std::string::npos : colon - at);
        if (!dir.empty()) roots.push_back(dir);
        if (colon == std::string::npos) break;
        at = colon + 1;
    }

    for (const std::string& root : roots) {
        std::error_code ec;
        if (fs::exists(fs::path(root) / "applications" / desktopId, ec) && !ec) return true;
    }
    return false;
}

struct SchemeHandler {
    const char* desktopId;
    const char* name;
    const char* mimeTypeLine;
    std::vector<const char*> schemes;
};

const std::vector<SchemeHandler>& installedHandlers() {
    static const std::vector<SchemeHandler> handlers = {
        {"tuxblox-player-handler.desktop", "TuxBlox Player",
         "x-scheme-handler/roblox;x-scheme-handler/roblox-player;",
         {"x-scheme-handler/roblox", "x-scheme-handler/roblox-player"}},
        {"tuxblox-studio-handler.desktop", "TuxBlox Studio",
         "x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;",
         {"x-scheme-handler/roblox-studio", "x-scheme-handler/roblox-studio-auth"}},
    };
    return handlers;
}

void runCommandBestEffort(const std::vector<std::string>& argv) {
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);

    pid_t pid = fork();
    if (pid < 0) {
        if (devnull >= 0) close(devnull);
        return;
    }
    if (pid == 0) {
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
        }
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    if (devnull >= 0) close(devnull);
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Makes TuxBlox the default for `mimeType`, unless somebody else's choice is already there.
void claimDefaultIfFree(const std::string& mimeType, const char* pDesktopId) {
    std::string current;
    for (const std::string& path : mimeappsCandidatePaths()) {
        current = explicitDefaultFor(readWholeFile(path), mimeType);
        if (!current.empty()) break;
    }
    if (!shouldClaimAssociation(current, desktopEntryInstalled(current))) return;
    runCommandBestEffort({"xdg-mime", "default", pDesktopId, mimeType});
}

// The file types TuxBlox opens in Studio. One handler covers all of them.
//
// A place is two types because the XML form is also XML and wants saying so, while a model is one type
// covering both its forms, which is how the rest of the Linux Roblox ecosystem already maps them -- a
// second, conflicting definition of the same extensions is worse than matching what is there.
const std::vector<const char*>& placeFileMimeTypes() {
    static const std::vector<const char*> types = {
        "application/x-roblox-place",      // .rbxl
        "application/x-roblox-place+xml",  // .rbxlx
        "application/x-roblox-model",      // .rbxm and .rbxmx
    };
    return types;
}

} // namespace

std::string explicitDefaultFor(const std::string& mimeappsText, const std::string& mimeType) {
    bool inDefaults = false;
    size_t at = 0;
    while (at < mimeappsText.size()) {
        const size_t eol = mimeappsText.find('\n', at);
        std::string line = mimeappsText.substr(at, eol == std::string::npos ? std::string::npos : eol - at);
        at = eol == std::string::npos ? mimeappsText.size() : eol + 1;

        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        if (line.empty() || line.front() == '#') continue;

        if (line.front() == '[') {
            inDefaults = line == "[Default Applications]";
            continue;
        }
        if (!inDefaults) continue;

        const size_t equals = line.find('=');
        // Matched on the whole key, so a longer type that starts the same way is a different entry.
        if (equals == std::string::npos || line.compare(0, equals, mimeType) != 0) continue;

        std::string value = line.substr(equals + 1);
        const size_t semicolon = value.find(';');
        if (semicolon != std::string::npos) value = value.substr(0, semicolon);
        while (!value.empty() && value.front() == ' ') value.erase(value.begin());
        while (!value.empty() && value.back() == ' ') value.pop_back();
        if (!value.empty()) return value;
    }
    return "";
}

bool shouldClaimAssociation(const std::string& currentDefault, bool currentDefaultInstalled) {
    if (currentDefault.empty()) return true;
    // Not installed, so it opens nothing -- a leftover rather than a choice, whoever left it.
    if (!currentDefaultInstalled) return true;
    // Put there by hand to test a build, so it outranks the installed entry.
    if (currentDefault == "tuxblox-player-dev.desktop" || currentDefault == "tuxblox-studio-dev.desktop") {
        return false;
    }
    // Already ours: setting it again costs nothing and repairs a half-written mimeapps.list.
    if (currentDefault.rfind("tuxblox-", 0) == 0) return true;
    return false;
}

void writeDesktopEntries(const std::string& launcherExePath) {
    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        static const char* kIconSizes[] = {"16x16", "24x24", "32x32", "48x48",
                                            "64x64", "96x96", "128x128", "256x256"};
        for (const char* size : kIconSizes) {
            const std::string iconThemeDir =
                std::string(home) + "/.local/share/icons/hicolor/" + size + "/apps";
            std::error_code ec;
            fs::create_directories(iconThemeDir, ec);
            if (ec) return;
            std::ofstream iconFile(iconThemeDir + "/tuxblox.png", std::ios::binary);
            if (!iconFile) return;
            iconFile.write(reinterpret_cast<const char*>(kTuxbloxLogoPng),
                            static_cast<std::streamsize>(kTuxbloxLogoPngLen));
            if (!iconFile) return;
        }

        std::error_code ec;
        fs::create_directories(appsDir, ec);
        if (ec) return;

        // Main entry, with quick-launch/documentation Desktop Actions.
        {
            std::ofstream f(appsDir + "/tuxblox-launcher.desktop");
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=TuxBlox\n"
                "Comment=Roblox on Linux\n"
                "GenericName=Roblox Client\n"
                "Keywords=Roblox;Player;Studio;Game;Wine;\n"
                "Exec=\"" << launcherExePath << "\"\n"
                "Icon=tuxblox\n"
                "Terminal=false\n"
                "StartupWMClass=tuxblox-launcher\n"
                "Categories=Game;\n"
                "Actions=Documentation;\n"
                "\n"
                "[Desktop Action Documentation]\n"
                "Name=Documentation\n"
                "Exec=xdg-open https://tuxblox.net/docs\n";
        }

        // URL-scheme handler entries (not shown in app grids).
        for (const auto& h : installedHandlers()) {
            std::ofstream f(appsDir + "/" + h.desktopId);
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=" << h.name << "\n"
                "Exec=\"" << launcherExePath << "\" %u\n"
                "Icon=tuxblox\n"
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=" << h.mimeTypeLine << "\n";
        }

        {
            const std::string mimePackagesDir = std::string(home) + "/.local/share/mime/packages";
            std::error_code mimeEc;
            fs::create_directories(mimePackagesDir, mimeEc);
            if (!mimeEc) {
                std::ofstream f(mimePackagesDir + "/tuxblox-roblox-place.xml");
                if (f) {
                    f <<
                        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
                        "  <mime-type type=\"application/x-roblox-place\">\n"
                        "    <comment>Roblox Place</comment>\n"
                        "    <glob pattern=\"*.rbxl\"/>\n"
                        "  </mime-type>\n"
                        "  <mime-type type=\"application/x-roblox-place+xml\">\n"
                        "    <comment>Roblox Place (XML)</comment>\n"
                        "    <sub-class-of type=\"text/xml\"/>\n"
                        "    <glob pattern=\"*.rbxlx\"/>\n"
                        "  </mime-type>\n"
                        "  <mime-type type=\"application/x-roblox-model\">\n"
                        "    <comment>Roblox Model</comment>\n"
                        "    <glob pattern=\"*.rbxm\"/>\n"
                        "    <glob pattern=\"*.rbxmx\"/>\n"
                        "  </mime-type>\n"
                        "</mime-info>\n";
                }
            }
        }

        {
            std::ofstream f(appsDir + "/tuxblox-studio-place.desktop");
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=Roblox Studio\n"
                "Comment=via TuxBlox\n"
                "Exec=\"" << launcherExePath << "\" --open-file %f\n"
                "Icon=tuxblox\n"
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=application/x-roblox-place;application/x-roblox-place+xml;application/x-roblox-model;\n";
        }

        {
            std::error_code rmEc;
            fs::remove(appsDir + "/tuxblox-url-handler.desktop", rmEc);
            fs::remove(appsDir + "/tuxblox-roblox-handler.desktop", rmEc);
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

void ensureDesktopIntegration(const std::string& launcherExePath, const std::string& installDir) {
    writeDesktopEntries(launcherExePath);

    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        exportPrefixShortcuts(installDir, launcherExePath);

        if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
            for (const auto& h : installedHandlers()) {
                for (const char* scheme : h.schemes) {
                    claimDefaultIfFree(scheme, h.desktopId);
                }
            }

            // Before claiming the file types: the types have to exist in the database before anything can be made their default.
            runCommandBestEffort({"update-mime-database", std::string(home) + "/.local/share/mime"});
            for (const char* mimeType : placeFileMimeTypes()) {
                claimDefaultIfFree(mimeType, "tuxblox-studio-place.desktop");
            }
            runCommandBestEffort({"update-desktop-database", appsDir});

            runCommandBestEffort({"gtk-update-icon-cache", std::string(home) + "/.local/share/icons/hicolor"});
        }

        if (isInsideDistrobox()) {
            runCommandBestEffort({"distrobox-export", "--app", "tuxblox-launcher"});
            for (const auto& h : installedHandlers()) {
                std::string exportId = h.desktopId;
                exportId.erase(exportId.size() - std::string(".desktop").size());
                runCommandBestEffort({"distrobox-export", "--app", exportId});
            }

            for (const char* exportId :
                 {"tuxblox-roblox-studio", "tuxblox-roblox-player", "tuxblox-studio-place"}) {
                runCommandBestEffort({"distrobox-export", "--app", exportId});
            }
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

} // namespace tuxblox
