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
#include <chrono>
#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <sys/wait.h>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <vector>

namespace fs = std::filesystem;

namespace tuxblox {

namespace {

std::string captureCommand(const std::vector<std::string>& argv) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "";
    }
    if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
    close(pipefd[1]);

    std::string result;
    char buf[256];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) result.append(buf, static_cast<size_t>(n));
    close(pipefd[0]);

    int status = 0;
    for (int i = 0; i < 30; ++i) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

std::string parseGioDefault(const std::string& out) {
    const size_t eol = out.find('\n');
    const std::string line = out.substr(0, eol == std::string::npos ? out.size() : eol);
    const size_t sep = line.rfind(": ");
    if (sep == std::string::npos) return "";
    std::string id = line.substr(sep + 2);
    while (!id.empty() && (id.back() == ' ' || id.back() == '\r')) id.pop_back();
    const std::string suffix = ".desktop";
    if (id.size() <= suffix.size()) return "";
    if (id.compare(id.size() - suffix.size(), suffix.size(), suffix) != 0) return "";
    if (id.find('/') != std::string::npos || id.find(' ') != std::string::npos) return "";
    return id;
}

std::string queryXdgMimeDefault(const std::string& scheme) {
    const std::string viaGio = parseGioDefault(captureCommand({"gio", "mime", scheme}));
    if (!viaGio.empty()) return viaGio;
    return captureCommand({"xdg-mime", "query", "default", scheme});
}

bool isKnownTuxBloxDevHandler(const std::string& desktopId) {
    return desktopId == "tuxblox-player-dev.desktop" ||
           desktopId == "tuxblox-studio-dev.desktop";
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

} // namespace

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
                "StartupWMClass=TuxBloxLauncher\n"
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
                "MimeType=application/x-roblox-place;application/x-roblox-place+xml;\n";
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
                    if (isKnownTuxBloxDevHandler(queryXdgMimeDefault(scheme))) continue;
                    runCommandBestEffort({"xdg-mime", "default", h.desktopId, scheme});
                }
            }

            runCommandBestEffort({"update-mime-database", std::string(home) + "/.local/share/mime"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-studio-place.desktop",
                                   "application/x-roblox-place"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-studio-place.desktop",
                                   "application/x-roblox-place+xml"});
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
