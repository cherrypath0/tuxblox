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
