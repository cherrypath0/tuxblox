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

#include "ui.h"
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include <SDL.h>
#include <SDL_opengl.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tuxblox_logo_png.h" // generated at build time: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include "inter_regular_ttf.h"  // generated at build time: kInterRegularTtf[], kInterRegularTtfLen
#include "inter_semibold_ttf.h" // generated at build time: kInterSemiBoldTtf[], kInterSemiBoldTtfLen

namespace {

// Multiplier applied to every pixel size/position/font size in this file so
// the window reads at the same visual size regardless of display
// resolution -- set once in Ui::init() from the desktop resolution relative
// to a 1440p baseline (see the SDL_GetDesktopDisplayMode call there).
float g_uiScale = 1.0f;

// Everything above the Cancel button (which starts at y=222 -- see the
// button's SetCursorPosY below) counts as "title bar" for drag purposes.
// Base (1440p, scale == 1.0) value -- scaled by g_uiScale in windowHitTest.
constexpr int kDragRegionHeight = 205;

// Lets the window manager/compositor handle a press-and-drag in the top
// portion of the window as a title-bar drag. Unlike manually polling
// SDL_GetGlobalMouseState + SDL_SetWindowPosition every frame, this is
// implemented by the platform's own interactive-move request (e.g. an
// xdg_toplevel move on Wayland), so it works correctly under native
// Wayland compositors too, which refuse to let a client reposition its
// own surface directly.
SDL_HitTestResult SDLCALL windowHitTest(SDL_Window*, const SDL_Point* area, void*) {
    return area->y < kDragRegionHeight * g_uiScale ? SDL_HITTEST_DRAGGABLE : SDL_HITTEST_NORMAL;
}

} // namespace

namespace tuxblox {

Ui::Ui() = default;
Ui::~Ui() { shutdown(); }

bool Ui::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return false;
    }

    // Every pixel size/position/font size in this file is authored against a
    // 1440p baseline (g_uiScale == 1.0 there); scale it by the desktop's
    // actual resolution relative to that baseline so the window occupies the
    // same proportion of the screen at any resolution, rather than a fixed
    // pixel count that reads tiny on 4K and oversized on 1080p. Clamped so
    // an unusual/multi-monitor display mode can't produce a degenerate
    // window.
    {
        SDL_DisplayMode mode;
        if (SDL_GetDesktopDisplayMode(0, &mode) == 0 && mode.h > 0) {
            g_uiScale = static_cast<float>(mode.h) / 1440.0f;
        }
        if (g_uiScale < 0.75f) g_uiScale = 0.75f;
        if (g_uiScale > 3.0f) g_uiScale = 3.0f;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("TuxBlox Installer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(480 * g_uiScale), static_cast<int>(280 * g_uiScale),
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_BORDERLESS);
    if (!window) return false;
    window_ = window;
    SDL_SetWindowHitTest(window, windowHitTest, nullptr);

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) return false;
    glContext_ = gl;
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    // A one-shot installer that exec()s away should not leave an imgui.ini
    // behind in whatever directory the user launched it from.
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 14.0f;
    style.ChildRounding = 14.0f;
    style.PopupRounding = 14.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    // Scales all of ImGui's own built-in metrics (padding, spacing, and the
    // rounding values set above) by the same factor as everything else in
    // this file.
    style.ScaleAllSizes(g_uiScale);

    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");

    // FontDataOwnedByAtlas=false: these arrays are `static const` data baked
    // into the binary, not heap allocations -- ImGui must not try to free them.
    ImFontConfig regularCfg;
    regularCfg.FontDataOwnedByAtlas = false;
    fontRegular_ = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kInterRegularTtf), static_cast<int>(kInterRegularTtfLen),
        16.0f * g_uiScale, &regularCfg);

    ImFontConfig semiBoldCfg;
    semiBoldCfg.FontDataOwnedByAtlas = false;
    fontSemiBold_ = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
        const_cast<unsigned char*>(kInterSemiBoldTtf), static_cast<int>(kInterSemiBoldTtfLen),
        19.0f * g_uiScale, &semiBoldCfg);

    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
        kTuxbloxLogoPng, static_cast<int>(kTuxbloxLogoPngLen), &logoWidth_, &logoHeight_, &channels, 4);
    if (pixels) {
        GLuint tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logoWidth_, logoHeight_, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        logoTexture_ = tex;

        // Reuse the same decoded pixels for the taskbar/alt-tab window icon
        // before freeing them -- SDL_SetWindowIcon copies the surface's
        // pixel data internally, so freeing iconSurface (and then pixels)
        // right after is safe.
        SDL_Surface* iconSurface = SDL_CreateRGBSurfaceWithFormatFrom(
            pixels, logoWidth_, logoHeight_, 32, logoWidth_ * 4, SDL_PIXELFORMAT_RGBA32);
        if (iconSurface) {
            SDL_SetWindowIcon(window, iconSurface);
            SDL_FreeSurface(iconSurface);
        }

        stbi_image_free(pixels);
    }

    return true;
}

void Ui::shutdown() {
    if (logoTexture_) {
        GLuint tex = logoTexture_;
        glDeleteTextures(1, &tex);
        logoTexture_ = 0;
    }
    if (glContext_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplSDL2_Shutdown();
        ImGui::DestroyContext();
        SDL_GL_DeleteContext(static_cast<SDL_GLContext>(glContext_));
        glContext_ = nullptr;
    }
    if (window_) {
        SDL_DestroyWindow(static_cast<SDL_Window*>(window_));
        window_ = nullptr;
    }
    SDL_Quit();
}

bool Ui::renderFrame(App& app) {
    auto* window = static_cast<SDL_Window*>(window_);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event);
        if (event.type == SDL_QUIT) {
            app.cancel();
            return false;
        }
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame();
    ImGui::NewFrame();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    int drawW, drawH;
    SDL_GL_GetDrawableSize(window, &drawW, &drawH);
    ImGui::GetIO().DisplayFramebufferScale = ImVec2(
        w > 0 ? static_cast<float>(drawW) / static_cast<float>(w) : 1.0f,
        h > 0 ? static_cast<float>(drawH) / static_cast<float>(h) : 1.0f);

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(w), static_cast<float>(h)));
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::Begin("##installer", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBackground);

    // Draw the panel as an explicit rounded rect instead of relying on
    // ImGui's flat WindowBg fill: AddRectFilled leaves the rect's own
    // corners untouched when rounding > 0, so the GL clear color shows
    // through at the window's true square corners -- the clear color below
    // is set to this exact same RGB value so those corners are invisible
    // (rather than merely "similar", which still reads as a visible seam).
    // True per-pixel window transparency (letting the desktop itself show
    // through) isn't reliably supported by SDL2 on Linux without extra
    // platform-specific work (a manual ARGB X11 visual, Wayland alpha
    // buffer negotiation); attempting it via a plain alpha clear produced
    // opaque black corners instead of transparency, which is worse -- so
    // this sticks with the seamless-color illusion instead.
    ImGui::GetWindowDrawList()->AddRectFilled(
        ImVec2(0.0f, 0.0f), ImVec2(static_cast<float>(w), static_cast<float>(h)),
        IM_COL32(28, 28, 33, 255), style.WindowRounding);

    auto snap = app.snapshot();

    if (logoTexture_) {
        float logoDisplaySize = 96.0f * g_uiScale;
        ImGui::SetCursorPosX((w - logoDisplaySize) * 0.5f);
        ImGui::SetCursorPosY(24.0f * g_uiScale);
        ImGui::Image((void*)(intptr_t)logoTexture_, ImVec2(logoDisplaySize, logoDisplaySize));
    }

    std::string statusText = "Starting...";
    switch (snap.phase) {
        case AppPhase::Init:             statusText = "Checking system requirements"; break;
        case AppPhase::FetchingManifest: statusText = "Contacting TuxBlox servers"; break;
        case AppPhase::Installing:       statusText = snap.currentStepLabel; break;
        case AppPhase::Error:            statusText = "Error"; break;
        case AppPhase::Done:             statusText = "Launching TuxBlox"; break;
    }

    ImGui::SetCursorPosY(148.0f * g_uiScale);
    ImGui::PushFont(fontSemiBold_);
    float textWidth = ImGui::CalcTextSize(statusText.c_str()).x;
    ImGui::SetCursorPosX((w - textWidth) * 0.5f);
    ImGui::TextUnformatted(statusText.c_str());
    ImGui::PopFont();

    ImGui::SetCursorPosX(40.0f * g_uiScale);
    ImGui::SetCursorPosY(182.0f * g_uiScale);
    // ProgressBar's size_arg does NOT consult PushItemWidth (that stack is
    // only read by CalcItemSize() when size.x == 0.0f) -- passing -1 here
    // used to mean "fill to 1px before the content region's right edge",
    // which ignored the width below entirely and made the bar hug the
    // right edge (~9px margin) while staying anchored 40px from the left.
    // Passing the width explicitly is the only way to get it symmetric.
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.18f, 0.62f, 0.97f, 1.0f));
    ImGui::ProgressBar(static_cast<float>(snap.overallPercent / 100.0), ImVec2(w - 80.0f * g_uiScale, 0));
    ImGui::PopStyleColor();

    float buttonWidth = 100.0f * g_uiScale;
    ImGui::SetCursorPosX((w - buttonWidth) * 0.5f);
    ImGui::SetCursorPosY(222.0f * g_uiScale);
    ImGui::PushFont(fontRegular_);
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
        app.cancel();
        cancelledByUser_ = true;
    }
    ImGui::PopFont();

    ImGui::End();

    ImGui::Render();
    glViewport(0, 0, drawW, drawH);
    glClearColor(0.10980392f, 0.10980392f, 0.12941176f, 1.0f); // matches IM_COL32(28, 28, 33, ...) panel fill above
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    if (cancelledByUser_) {
        return false;
    }
    if (snap.phase == AppPhase::Error && !errorShown_) {
        errorShown_ = true;
        std::string message = "TuxBlox Installer has encountered an error and has to quit!\n\nDetails: " + snap.errorMessage;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Fatal Error",
            message.c_str(), window);
        return false;
    }
    if (snap.phase == AppPhase::Done) {
        return false;
    }

    return true;
}

} // namespace tuxblox
