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
