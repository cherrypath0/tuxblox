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
#include "ui_scale.h"
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

    // Sizes here are authored for 96 DPI. Scaling by the display's DPI keeps
    // the window one physical size everywhere; scaling by resolution did not,
    // because a 15" 1080p laptop and a 32" 1440p monitor have very different
    // DPI at similar pixel counts.
    {
        float dpi = 0.0f;
        if (SDL_GetDisplayDPI(0, nullptr, nullptr, &dpi) != 0) dpi = 0.0f;
        SDL_DisplayMode mode;
        const int height = SDL_GetDesktopDisplayMode(0, &mode) == 0 ? mode.h : 0;
        g_uiScale = computeUiScale(dpi, height);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window* window = SDL_CreateWindow("TuxBlox",
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
        // The logo is 440px square and draws at ~96px, so it is always
        // minified. Without mipmaps that samples a fraction of the source
        // pixels and the edges break up.
        // glGenerateMipmap is OpenGL 3.0; SDL_opengl.h only declares 1.1, so
        // the entry point is resolved at runtime. Without it the logo keeps
        // the plain linear filter and simply looks as it did before.
        auto generateMipmap = reinterpret_cast<void (APIENTRY*)(GLenum)>(
            SDL_GL_GetProcAddress("glGenerateMipmap"));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                        generateMipmap ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, logoWidth_, logoHeight_, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        if (generateMipmap) generateMipmap(GL_TEXTURE_2D);
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
    ImGui::Begin("##bootstrapper", nullptr,
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

    const std::string statusText =
        snap.phase == Phase::Error ? std::string("Error") : snap.status;

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
    // Empty overlay: ImGui draws "42%" inside the bar otherwise.
    // Height given explicitly: left at 0 the bar takes the font size plus
    // frame padding, which reads chunkier than it needs to.
    ImGui::ProgressBar(static_cast<float>(snap.overallPercent / 100.0),
                       ImVec2(w - 80.0f * g_uiScale, 16.0f * g_uiScale), "");
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
    if (snap.phase == Phase::Error && !errorShown_) {
        errorShown_ = true;
        const std::string message =
            "TuxBlox could not finish updating Roblox.\n\nDetails: " + snap.errorMessage;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "TuxBlox", message.c_str(), window);
        return false;
    }
    // Preview stays up so the window can actually be looked at; the working
    // modes close themselves, because the launcher is waiting on this process.
    if (snap.phase == Phase::Done && !app.holdsOpenWhenDone()) {
        return false;
    }

    return true;
}

} // namespace tuxblox
