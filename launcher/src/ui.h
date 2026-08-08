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
#include "tray_icon.h"
#include <vector>

struct ImFont;

namespace tuxblox {

class Ui {
public:
    Ui();
    ~Ui();

    bool init();
    void shutdown();
    bool renderFrame(App& app);

private:
    void* window_ = nullptr;
    void* glContext_ = nullptr;
    unsigned int logoTexture_ = 0;
    int logoWidth_ = 0, logoHeight_ = 0;
    unsigned int playerIconTexture_ = 0;
    unsigned int studioIconTexture_ = 0;
    unsigned int homeIconTexture_ = 0;
    unsigned int infoIconTexture_ = 0;
    unsigned int globeIconTexture_ = 0;
    unsigned int docsIconTexture_ = 0;
    unsigned int githubIconTexture_ = 0;
    unsigned int discordIconTexture_ = 0;
    unsigned int settingsIconTexture_ = 0;
    ImFont* fontRegular_ = nullptr;
    ImFont* fontSemiBold_ = nullptr;
    bool shouldClose_ = false;
    bool containerWarningShown_ = false;

    // Set once the app enters background/tray mode (App::isBackgrounded())
    // -- drives hiding the main window and standing up trayIcon_ in its
    // place. See renderFrame()'s handling of App::isBackgrounded().
    bool windowHidden_ = false;
    TrayIcon trayIcon_;
    std::vector<unsigned char> trayIconRgba_; // prepared once in init(), see loadTrayIconRgba()
    int trayIconSize_ = 0;

    // Settings tab InputText backing buffers. Populated once from the
    // App's settings the first time the tab is rendered (settingsBuffersInitialized_
    // guards that) -- ImGui's InputText needs a stable buffer across
    // frames, and nothing else writes these fields while the UI is open.
    char protonEnvVarsBuf_[512] = {};
    char globalEnvVarsBuf_[512] = {};
    bool settingsBuffersInitialized_ = false;
};

} // namespace tuxblox
