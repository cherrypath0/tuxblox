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
#include <chrono>
#include <cstdlib>
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

// Runs "xdg-mime query default <scheme>" and returns its stdout (trimmed),
// or "" if the scheme has no default / the query failed. Same no-shell
// fork+exec style as runCommandBestEffort, but this one needs the child's
// stdout back, so it pipes it through instead of firing-and-forgetting.
std::string queryXdgMimeDefault(const std::string& scheme) {
    int pipefd[2];
    if (pipe(pipefd) != 0) return "";

    std::vector<std::string> argv = {"xdg-mime", "query", "default", scheme};
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

// The repo-local dev workflow (./install-handler.sh, launch.sh %u) writes
// these IDs. When one of them is the current default for a scheme, that
// was a deliberate choice by whoever's doing dev/testing work against the
// repo prefix -- ensureDesktopIntegration() below must not silently revert
// it back to the installed handler every time the GUI happens to start.
bool isKnownTuxBloxDevHandler(const std::string& desktopId) {
    return desktopId == "tuxblox-roblox-dev.desktop" ||
           desktopId == "tuxblox-player-dev.desktop" ||
           desktopId == "tuxblox-studio-dev.desktop";
}

struct SchemeHandler {
    const char* desktopId;
    const char* name;
    const char* mimeTypeLine;              // full MimeType= value written to the .desktop file
    std::vector<const char*> schemes;      // same schemes, split out for the xdg-mime default loop
};

// One file per Name -- a .desktop entry only has a single Name=, and each
// of these should read distinctly in "Open With" pickers / xdg-mime query
// output rather than one generic "URL Handler" covering all three.
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

    pid_t pid = fork();
    if (pid < 0) return;
    if (pid == 0) {
        execvp(cargv[0], cargv.data());
        _exit(127);
    }
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

} // namespace

void writeDesktopEntries(const std::string& launcherExePath) {
    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        // kTuxbloxLogoPng is already fully embedded in this binary (fetched
        // and rasterized once at build time -- see FetchLogo.cmake); this
        // write is not loading a separate asset, it's just producing the
        // one real file .desktop Icon= entries are required to point at
        // (the XDG desktop-entry spec has no way to reference bytes inside
        // a binary directly). Installed under the standard per-user icon
        // theme location, so Icon= can name it ("tuxblox") instead of
        // hardcoding an absolute path -- proper icon-theme lookup/scaling,
        // and it stops being something that has to live under installDir at
        // all.
        //
        // Written into EVERY standard hicolor size bucket, not just 256x256:
        // the actual embedded image is a fixed 440x440 raster (no scalable
        // SVG available, see FetchLogo.cmake's own history), and some icon
        // loaders resolve a specific requested size (e.g. 16/24/32/48, the
        // sizes a titlebar/taskbar typically ask for) strictly against
        // whatever bucket exists rather than falling back across sizes --
        // leaving only 256x256 populated meant every one of those smaller,
        // more commonly-requested lookups came up empty. All buckets get
        // the exact same bytes (oversized for the smaller ones); loaders
        // scale down a too-large icon far more reliably than they
        // synthesize a missing one.
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
                "Comment=Launch TuxBlox (Roblox on Linux via Proton)\n"
                "Exec=\"" << launcherExePath << "\"\n"
                "Icon=tuxblox\n"
                "Terminal=false\n"
                // Must match the WM_CLASS Qt's xcb backend actually emits --
                // on X11 that's the running binary's basename ("TuxBloxLauncher"),
                // not qapp.setDesktopFileName()'s "tuxblox-launcher" (that only
                // governs Wayland's xdg_toplevel app_id and icon-theme name
                // fallback, not X11 WM_CLASS). A mismatch here is why desktop
                // environments were pinning/taskbar-matching this launcher with
                // a blank/generic icon instead of tuxblox.png.
                "StartupWMClass=TuxBloxLauncher\n"
                "Categories=Game;\n"
                "Actions=LaunchPlayer;LaunchStudio;Documentation;\n"
                "\n"
                "[Desktop Action LaunchPlayer]\n"
                "Name=Launch Roblox Player\n"
                "Exec=\"" << launcherExePath << "\" --launch-player\n"
                "\n"
                "[Desktop Action LaunchStudio]\n"
                "Name=Launch Roblox Studio\n"
                "Exec=\"" << launcherExePath << "\" --launch-studio\n"
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
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=" << h.mimeTypeLine << "\n";
        }

        // Superseded by the per-scheme files above -- remove so it doesn't
        // linger as a dead duplicate entry claiming the same MimeTypes.
        {
            std::error_code rmEc;
            fs::remove(appsDir + "/tuxblox-url-handler.desktop", rmEc);
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

void ensureDesktopIntegration(const std::string& launcherExePath) {
    writeDesktopEntries(launcherExePath);

    try {
        const char* home = std::getenv("HOME");
        if (!home || home[0] == '\0') return;
        const std::string appsDir = std::string(home) + "/.local/share/applications";

        if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
            for (const auto& h : installedHandlers()) {
                for (const char* scheme : h.schemes) {
                    // Don't stomp a deliberate switch to the repo-local dev
                    // handler (see isKnownTuxBloxDevHandler above) -- only
                    // (re)claim the scheme if it's unset or held by
                    // something else (a stale/foreign association is still
                    // self-healed).
                    if (isKnownTuxBloxDevHandler(queryXdgMimeDefault(scheme))) continue;
                    runCommandBestEffort({"xdg-mime", "default", h.desktopId, scheme});
                }
            }
            runCommandBestEffort({"update-desktop-database", appsDir});
            // Best-effort, same reasoning as update-desktop-database above --
            // not every desktop environment needs this to pick up a newly
            // added icon-theme file, but GTK-based ones (and the icon
            // picker in some app launchers) can otherwise keep showing a
            // generic icon until the theme cache is rebuilt.
            runCommandBestEffort({"gtk-update-icon-cache", std::string(home) + "/.local/share/icons/hicolor"});
        }

        // Inside a Distrobox container, ~/.local/share/applications is
        // shared with the host (Distrobox bind-mounts $HOME by default),
        // so both .desktop files above are already visible on the host's
        // app menu -- but their Exec= lines are only valid *inside* the
        // container. Best-effort: invoke `distrobox-export --app` for each
        // so the host can launch them correctly (the export mechanism
        // itself -- whether it rewrites these entries' Exec= in place or
        // creates separate host-side entries -- has not been verified
        // against a real Distrobox install; confirm the actual on-disk
        // result with a real container before relying on this for end
        // users). Best-effort, same as the xdg-mime calls above: if the
        // binary isn't on PATH, runCommandBestEffort's execvp ENOENT
        // handling exits 127 harmlessly.
        if (isInsideDistrobox()) {
            runCommandBestEffort({"distrobox-export", "--app", "tuxblox-launcher"});
            for (const auto& h : installedHandlers()) {
                std::string exportId = h.desktopId;
                exportId.erase(exportId.size() - std::string(".desktop").size());
                runCommandBestEffort({"distrobox-export", "--app", exportId});
            }
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

} // namespace tuxblox
