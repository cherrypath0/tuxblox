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

#include "desktop_shortcut.h"
#include "tuxblox_logo_png.h" // generated at build time: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include "container_env.h"
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

void runCommandBestEffort(const std::vector<std::string>& argv) {
    // Build the argv array *before* fork() -- allocating (std::vector's
    // reserve/push_back) in the child of a multithreaded process is a known
    // deadlock hazard: another thread could hold the malloc arena lock at
    // the exact moment of fork(), and that lock is never released in the
    // child. Only touch the already-built, non-allocating pointer array
    // after fork().
    std::vector<char*> cargv;
    cargv.reserve(argv.size() + 1);
    for (const auto& a : argv) cargv.push_back(const_cast<char*>(a.c_str()));
    cargv.push_back(nullptr);

    // Silence these helpers. Their exit status is already ignored, but their
    // chatter (gtk-update-icon-cache's "No theme index file.", xdg-mime's
    // "qtpaths: command not found") is inherited straight onto our stdout/
    // stderr, where under --headless it lands in the middle of the progress
    // output. Opened before fork() so the child does no allocation of its
    // own; O_CLOEXEC drops this fd across the exec while the dup2'd 1 and 2
    // survive it (dup2 clears CLOEXEC on the new descriptor).
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
    // Bounded, non-blocking wait -- xdg-mime/update-desktop-database talk to
    // a D-Bus session that can hang (this repo hit exactly this failure mode
    // once before, see 09b369a13). Give it up to ~3s, then give up rather
    // than block the caller indefinitely; we deliberately don't kill a
    // straggler process afterward -- it's harmless to leave running, and
    // this is best-effort desktop integration, not worth SIGKILL complexity.
    for (int i = 0; i < 30; ++i) {
        int status = 0;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid || r < 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

struct SchemeHandler {
    const char* desktopId;
    const char* name;
    const char* mimeTypeLine;          // full MimeType= value written to the .desktop file
    std::vector<const char*> schemes;  // same schemes, split out for the xdg-mime default loop
};

// Mirrors launcher/src/desktop_integration.cpp's installedHandlers() --
// keep the two in sync if either the handler set or naming changes.
const std::vector<SchemeHandler>& installedHandlers() {
    static const std::vector<SchemeHandler> handlers = {
        {"tuxblox-roblox-handler.desktop", "TuxBlox",
         "x-scheme-handler/roblox;", {"x-scheme-handler/roblox"}},
        {"tuxblox-player-handler.desktop", "TuxBlox Player",
         "x-scheme-handler/roblox-player;", {"x-scheme-handler/roblox-player"}},
        {"tuxblox-studio-handler.desktop", "TuxBlox Studio",
         "x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;",
         {"x-scheme-handler/roblox-studio", "x-scheme-handler/roblox-studio-auth"}},
    };
    return handlers;
}

} // namespace

void createDesktopShortcut(const std::string& launcherExePath) {
    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        // kTuxbloxLogoPng is already a complete, valid .png file's raw bytes
        // (FetchLogo.cmake downloads the icon PNG, then BinToHeader.cmake
        // embeds that file's bytes verbatim) -- so this is a direct
        // byte-for-byte write, no re-encoding needed.
        // Written under the standard per-user icon theme location/size
        // bucket (not installDir) so the .desktop entry below can name it
        // ("tuxblox") instead of hardcoding an absolute path -- proper
        // icon-theme lookup/scaling, and it's no longer tied to installDir
        // existing at all. The asset is 440x440 rather than an exact 256
        // match for this bucket; icon-theme lookup scales it down, and
        // staying in 256x256/apps keeps the path uninstall.cpp removes (and
        // the one prior installs already wrote) unchanged.
        const std::string iconThemeDir = std::string(home) + "/.local/share/icons/hicolor/256x256/apps";
        std::error_code ec;
        fs::create_directories(iconThemeDir, ec);
        if (ec) return;
        {
            std::ofstream iconFile(iconThemeDir + "/tuxblox.png", std::ios::binary);
            if (!iconFile) return;
            iconFile.write(reinterpret_cast<const char*>(kTuxbloxLogoPng),
                            static_cast<std::streamsize>(kTuxbloxLogoPngLen));
            if (!iconFile) return;
        }

        fs::create_directories(appsDir, ec);
        if (ec) return;

        const std::string desktopPath = appsDir + "/tuxblox-launcher.desktop";
        std::ofstream desktopFile(desktopPath);
        if (!desktopFile) return;
        desktopFile <<
            "[Desktop Entry]\n"
            "Type=Application\n"
            "Name=TuxBlox Launcher\n"
            "Comment=Launch TuxBlox (Roblox on Linux via Proton)\n"
            // The full executable path as resolved by the install pipeline,
            // not installDir + "/TuxBloxLauncher": an archive-shaped launcher
            // artifact extracts into its own directory, so the binary lives a
            // level below installDir. Same value refreshUrlHandlers() writes.
            "Exec=\"" << launcherExePath << "\"\n"
            "Icon=tuxblox\n"
            "Terminal=false\n"
            "Categories=Game;\n";
        desktopFile.close();
        if (!desktopFile) return;

        // Best-effort, same reasoning as desktop_integration.cpp's identical
        // call -- not every desktop environment needs this to pick up a
        // newly added icon-theme file, but GTK-based ones can otherwise
        // keep showing a generic icon until the theme cache is rebuilt.
        runCommandBestEffort({"gtk-update-icon-cache", std::string(home) + "/.local/share/icons/hicolor"});

        // Inside a Distrobox container, ~/.local/share/applications is
        // shared with the host (Distrobox bind-mounts $HOME by default),
        // so the .desktop file above is already visible on the host's app
        // menu -- but its Exec= path is only valid *inside* the container.
        // Best-effort: invoke `distrobox-export --app` so the host can
        // launch it correctly (the export mechanism itself -- whether it
        // rewrites this entry's Exec= in place or creates a separate
        // host-side entry -- has not been verified against a real
        // Distrobox install; confirm the actual on-disk result with a
        // real container before relying on this for end users). If the
        // binary isn't on PATH, execvp's ENOENT handling above just exits
        // 127 harmlessly, same as this file's existing behavior when e.g.
        // an icon write fails.
        if (isInsideDistrobox()) {
            runCommandBestEffort({"distrobox-export", "--app", "tuxblox-launcher"});
        }
    } catch (...) {
        // Best-effort -- a missing shortcut must not fail an otherwise
        // successful install.
    }
}

void refreshUrlHandlers(const std::string& launcherExePath) {
    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";
        std::error_code ec;
        fs::create_directories(appsDir, ec);
        if (ec) return;

        // Remove first, then set again: an install/update is a deliberate
        // top-level action, so it should force TuxBlox back as the default
        // even over an active repo-local dev handler, and it should never
        // leave a stale file from a previous version's naming scheme
        // behind (e.g. the pre-rename shared "tuxblox-url-handler.desktop").
        for (const auto& h : installedHandlers()) fs::remove(appsDir + "/" + h.desktopId, ec);
        fs::remove(appsDir + "/tuxblox-url-handler.desktop", ec);

        for (const auto& h : installedHandlers()) {
            std::ofstream f(appsDir + "/" + h.desktopId);
            if (!f) continue;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=" << h.name << "\n"
                "Exec=\"" << launcherExePath << "\" %u\n"
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=" << h.mimeTypeLine << "\n";
        }

        if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
            for (const auto& h : installedHandlers()) {
                for (const char* scheme : h.schemes) {
                    runCommandBestEffort({"xdg-mime", "default", h.desktopId, scheme});
                }
            }
            runCommandBestEffort({"update-desktop-database", appsDir});
        }

        if (isInsideDistrobox()) {
            for (const auto& h : installedHandlers()) {
                std::string exportId = h.desktopId;
                exportId.erase(exportId.size() - std::string(".desktop").size());
                runCommandBestEffort({"distrobox-export", "--app", exportId});
            }
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-successful install/update.
    }
}

} // namespace tuxblox
