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

#include "app.h"
#include "cli.h"
#include "console_ui.h"
#include "ui.h"
#include "uninstall.h"
#include "version.h"
#include <SDL.h>
#include <cstdio>
#include <string>
#include <unistd.h>

namespace {
// Without --headless this is a windowed, double-clickable app -- there is no
// terminal to read stderr from, so failures have to surface as a native
// message box.
constexpr const char* kErrorTitle =
    "TuxBlox Installer has encountered an error and has to quit!";

// Reports a fatal error the way the current mode can actually be seen in:
// stderr under --headless (where SDL is never initialized at all, so the
// installer runs with no display), a message box otherwise.
void reportError(bool headless, const std::string& details) {
    if (headless) {
        fprintf(stderr, "Error: %s\n", details.c_str());
    } else {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kErrorTitle,
            ("Details: " + details).c_str(), nullptr);
    }
}
} // namespace

int main(int argc, char** argv) {
    using namespace tuxblox;

    const CliOptions options = parseArgs(argc, argv);
    if (!options.error.empty()) {
        fprintf(stderr, "%s\n\n%s", options.error.c_str(), usageText());
        return 2;
    }
    if (options.help) {
        printf("%s", usageText());
        return 0;
    }
    if (options.version) {
        printf("%s\n", kTuxBloxVersion);
        return 0;
    }

    // --uninstall -- passed by the launcher's Settings tab. Never shows the
    // install UI, just does the removal and reports the result.
    if (options.uninstall) {
        const bool ok = performUninstall();
        const char* failureText =
            "Desktop shortcuts and URL handlers were removed, but ~/.tuxblox could not be "
            "fully deleted. You may need to remove it manually.";
        if (options.headless) {
            if (ok) {
                printf("TuxBlox has been completely removed from this system.\n");
            } else {
                fprintf(stderr, "Error: %s\n", failureText);
            }
        } else if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            if (ok) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "TuxBlox Uninstalled",
                    "TuxBlox has been completely removed from this system.", nullptr);
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TuxBlox Error", failureText,
                    nullptr);
            }
            SDL_Quit();
        }
        return ok ? 0 : 1;
    }

    App app(options.channel);
    app.start();

    if (options.headless) {
        if (!runConsoleInstall(app)) {
            app.cancel(); // no-op if it already finished; unblocks a cancelled run
            return 1;
        }
    } else {
        Ui ui;
        if (!ui.init()) {
            reportError(false, "Failed to initialize installer UI (SDL2/OpenGL)");
            app.cancel();
            return 1;
        }

        bool running = true;
        while (running) {
            running = ui.renderFrame(app);
            if (app.readyToLaunch()) {
                break;
            }
        }

        ui.shutdown();

        if (!app.readyToLaunch()) {
            app.cancel(); // ensure the background thread unblocks if the window was closed early
            return 0;
        }
    }

    const std::string launcher = app.launcherPath();

    // --nolaunch -- the install is complete either way; the only difference
    // is that we stop here instead of handing off to the launcher.
    if (options.noLaunch) {
        // printf("Launcher not started (--nolaunch). Run it manually: %s\n", launcher.c_str());
        return 0;
    }

    execl(launcher.c_str(), launcher.c_str(), (char*)nullptr);
    // Only reached if execl() failed -- the install itself already succeeded,
    // so say so rather than letting the window just vanish.
    reportError(options.headless,
        "TuxBlox was installed successfully, but failed to launch " + launcher +
        ". You can try running it manually.");
    return 1;
}
