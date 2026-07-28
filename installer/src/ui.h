#pragma once
#include "app.h"

// Forward declare ImFont from global namespace (defined in imgui.h)
struct ImFont;

namespace tuxblox {

// Owns SDL/ImGui resources and renders one frame of the install dialog
// based on `app`'s current snapshot. Returns false when the window should
// close (user closed it, or clicked OK on the error popup, or Cancel).
class Ui {
public:
    Ui();
    ~Ui();

    bool init(); // creates window, GL context, ImGui context, loads logo texture
    void shutdown();

    // Processes SDL events and renders one frame. Returns false if the
    // application should exit after this frame.
    bool renderFrame(App& app);

private:
    void* window_ = nullptr;
    void* glContext_ = nullptr;
    unsigned int logoTexture_ = 0;
    int logoWidth_ = 0;
    int logoHeight_ = 0;
    bool cancelledByUser_ = false;
    bool errorShown_ = false; // guards SDL_ShowSimpleMessageBox to only show once
    ImFont* fontRegular_ = nullptr;
    ImFont* fontSemiBold_ = nullptr;
};

} // namespace tuxblox
