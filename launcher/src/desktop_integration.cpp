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

void ensureDesktopIntegration(const std::string& launcherExePath) {
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
        // theme location/size bucket rather than next to the install
        // directory, so Icon= can name it ("tuxblox") instead of hardcoding
        // an absolute path -- proper icon-theme lookup/scaling, and it
        // stops being something that has to live under installDir at all.
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
                // Must match the SDL_VIDEO_X11_WMCLASS value set in Ui::init()
                // -- lets desktop environments match the running window back
                // to this pinned launcher, so closing it doesn't leave the
                // taskbar/dock pin showing a blank icon.
                "StartupWMClass=tuxblox-launcher\n"
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

        // URL-scheme handler entry (not shown in app grids).
        {
            std::ofstream f(appsDir + "/tuxblox-url-handler.desktop");
            if (!f) return;
            f <<
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=TuxBlox URL Handler\n"
                "Exec=\"" << launcherExePath << "\" %u\n"
                "NoDisplay=true\n"
                "Terminal=false\n"
                "MimeType=x-scheme-handler/roblox-player;x-scheme-handler/roblox-studio;x-scheme-handler/roblox-studio-auth;\n";
        }

        if (!std::getenv("TUXBLOX_SKIP_XDG_MIME")) { // escape hatch for sandboxed test/CI runs
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-player"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-studio"});
            runCommandBestEffort({"xdg-mime", "default", "tuxblox-url-handler.desktop", "x-scheme-handler/roblox-studio-auth"});
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
            runCommandBestEffort({"distrobox-export", "--app", "tuxblox-url-handler"});
        }
    } catch (...) {
        // Best-effort -- must never fail an otherwise-working launch.
    }
}

} // namespace tuxblox
