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
#include <SDL.h>
#include <string>
#include <unistd.h>

namespace {
// This is a windowed, double-clickable app -- there is no terminal to read
// stderr from, so failures have to surface as a native message box.
constexpr const char* kErrorTitle =
    "TuxBlox Installer has encountered an error and has to quit!";
} // namespace

int main(int, char**) {
    using namespace tuxblox;

    App app;
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
