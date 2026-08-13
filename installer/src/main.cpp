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
#include "ui.h"
#include "uninstall.h"
#include <SDL.h>
#include <string>
#include <unistd.h>

namespace {
// This is a windowed, double-clickable app -- there is no terminal to read
// stderr from, so failures have to surface as a native message box.
constexpr const char* kErrorTitle =
    "TuxBlox Installer has encountered an error and has to quit!";
} // namespace

int main(int argc, char** argv) {
    using namespace tuxblox;

    // --uninstall -- passed by the launcher's Settings tab. Headless, same
    // as this binary already is for none of its other modes (it's always
    // windowed) -- but this one specifically must not show the install UI
    // at all, just do the removal and report the result.
    if (argc > 1 && std::string(argv[1]) == "--uninstall") {
        bool ok = performUninstall();
        if (SDL_Init(SDL_INIT_VIDEO) == 0) {
            if (ok) {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_INFORMATION, "TuxBlox Uninstalled",
                    "TuxBlox has been completely removed from this system.", nullptr);
            } else {
                SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TuxBlox Error",
                    "Desktop shortcuts and URL handlers were removed, but ~/.tuxblox could not be "
                    "fully deleted. You may need to remove it manually.", nullptr);
            }
            SDL_Quit();
        }
        return ok ? 0 : 1;
    }

    // --channel <name> -- passed by the launcher during an upgrade handoff
    // so the install stays on whatever channel the user picked in
    // Settings. Defaults to "stable" when run standalone (a fresh,
    // directly-downloaded install with no channel info available).
    std::string channel = "stable";
    for (int i = 1; i + 1 < argc; ++i) {
        if (std::string(argv[i]) == "--channel") {
            channel = argv[i + 1];
            break;
        }
    }

    App app(channel);
    app.start();

    Ui ui;
    if (!ui.init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kErrorTitle,
            "Details: Failed to initialize installer UI (SDL2/OpenGL)", nullptr);
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

    if (app.readyToLaunch()) {
        std::string launcher = app.launcherPath();
        execl(launcher.c_str(), launcher.c_str(), (char*)nullptr);
        // Only reached if execl() failed -- the install itself already
        // succeeded, so say so rather than letting the window just vanish.
        std::string msg = "Details: TuxBlox was installed successfully, but failed to launch " +
                          launcher + ". You can try running it manually.";
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kErrorTitle, msg.c_str(), nullptr);
        return 1;
    }

    app.cancel(); // ensure the background thread unblocks if the window was closed early
    return 0;
}
