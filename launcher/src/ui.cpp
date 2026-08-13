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
#include <cfloat>
#include <cstdlib>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "tuxblox_logo_png.h"        // generated: kTuxbloxLogoPng[], kTuxbloxLogoPngLen
#include "tuxblox_window_icon_png.h" // generated: kTuxbloxWindowIconPng[], kTuxbloxWindowIconPngLen
#include "roblox_player_icon_png.h"  // generated: kRobloxPlayerIconPng[], kRobloxPlayerIconPngLen
#include "roblox_studio_icon_png.h"  // generated: kRobloxStudioIconPng[], kRobloxStudioIconPngLen
#include "icon_home_png.h"           // generated: kIconHomePng[], kIconHomePngLen
#include "icon_info_png.h"           // generated: kIconInfoPng[], kIconInfoPngLen
#include "icon_globe_png.h"          // generated: kIconGlobePng[], kIconGlobePngLen
#include "icon_docs_png.h"           // generated: kIconDocsPng[], kIconDocsPngLen
#include "icon_github_png.h"         // generated: kIconGithubPng[], kIconGithubPngLen
#include "icon_discord_png.h"        // generated: kIconDiscordPng[], kIconDiscordPngLen
#include "icon_settings_png.h"       // generated: kIconSettingsPng[], kIconSettingsPngLen
#include "icon_privacy_png.h"        // generated: kIconPrivacyPng[], kIconPrivacyPngLen
#include "inter_regular_ttf.h"       // generated: kInterRegularTtf[], kInterRegularTtfLen
#include "inter_semibold_ttf.h"      // generated: kInterSemiBoldTtf[], kInterSemiBoldTtfLen
#include "version.h"                 // generated: tuxblox::kTuxBloxVersion

namespace tuxblox {

namespace {

// Multiplier applied to every pixel size/position/font size in this file so
// the whole UI reads at the same visual size regardless of display
// resolution -- set once in Ui::init() from the desktop resolution relative
// to a 1440p baseline (see the SDL_GetDesktopDisplayMode call there). All
// the constants below are the baseline (1440p, scale == 1.0) values; callers
// multiply by g_uiScale at the point of use.
float g_uiScale = 1.0f;

constexpr float kSidebarWidth = 160.0f;
constexpr float kToggleWidth = 44.0f;
constexpr float kToggleHeight = 24.0f;
constexpr ImVec4 kAccent(0.18f, 0.62f, 0.97f, 1.0f);
// Sidebar is a visibly distinct, slightly darker panel than the content
// area (rather than blending into it), with a thin separator between the
// two -- and its two entries are always a solid color (blue when
// selected, grey otherwise) instead of only showing feedback on hover, so
// it's never ambiguous which tab is active.
constexpr ImVec4 kSidebarBg(20.0f / 255.0f, 20.0f / 255.0f, 24.0f / 255.0f, 1.0f);
constexpr ImVec4 kSidebarItemSelected = kAccent;
constexpr ImVec4 kSidebarItemUnselected(0.30f, 0.30f, 0.33f, 1.0f);
constexpr ImU32 kSeparatorColor = IM_COL32(255, 255, 255, 20);
constexpr ImVec4 kErrorBannerBg(0.35f, 0.16f, 0.16f, 1.0f);
constexpr ImVec4 kErrorBannerText(1.0f, 0.78f, 0.78f, 1.0f);
// Uninstall button: a muted red at rest, brighter/more urgent once armed by
// a first click -- the color change itself is part of the "this is now one
// click from actually happening" signal, on top of the label text change.
constexpr ImVec4 kDangerColor(0.5f, 0.18f, 0.18f, 1.0f);
constexpr ImVec4 kDangerColorArmed(0.75f, 0.20f, 0.20f, 1.0f);
// Explicit transparent resting background for rows that should stay
// invisible until hovered (About's link rows) -- passing nullptr for
// renderIconRow's bgColor instead would fall through to Dear ImGui's own
// default ImGuiCol_Button, which happens to be a semi-transparent blue
// itself and reads as "every row is permanently selected".
constexpr ImVec4 kTransparent(0.0f, 0.0f, 0.0f, 0.0f);

// Opens `url` in the user's default browser via xdg-open, without leaving a
// zombie behind: double-forks so the immediate child (which we do wait on)
// exits right away, while the grandchild (the actual xdg-open) is
// re-parented to init.
void openUrl(const char* url) {
    pid_t pid = fork();
    if (pid == 0) {
        pid_t inner = fork();
        if (inner == 0) {
            setsid();
            execlp("xdg-open", "xdg-open", url, nullptr);
            _exit(127);
        }
        _exit(0);
    } else if (pid > 0) {
        int status = 0;
        waitpid(pid, &status, 0);
    }
}

// glGenerateMipmap is OpenGL 3.0+/ARB_framebuffer_object -- <SDL_opengl.h>
// only declares the legacy 1.1 functions statically linkable via libGL, so
// it has to be resolved as a function pointer instead. Safe to resolve
// lazily via a function-local static: first call happens inside Ui::init(),
// after SDL_GL_MakeCurrent() and single-threaded (no render/update threads
// exist yet), so there's no first-call race to guard against.
void generateMipmap(GLenum target) {
    using GenerateMipmapFn = void (*)(GLenum);
    static GenerateMipmapFn fn =
        reinterpret_cast<GenerateMipmapFn>(SDL_GL_GetProcAddress("glGenerateMipmap"));
    if (fn) fn(target);
}

// glGenerateMipmap's box filter averages RGB straight across alpha, ignoring
// it -- so a fully-transparent texel's (0,0,0) RGB (stb_image's decode of
// "no color data here") bleeds into neighboring opaque texels once a mip
// level's filter footprint reaches them, producing a dark ring right around
// the icon that gets more visible (reads as a blurry border) the further a
// texture is downscaled. Fix at the source: before mipmapping, spread each
// opaque texel's RGB a few pixels out into the fully-transparent texels
// around it (alpha stays 0, so this is invisible at full res) so the box
// filter always has a sensible color to average with near an edge. Iterates
// out from the existing color a ring at a time -- enough passes to outrun
// the ~2x-per-level footprint growth for the mip levels these icons are
// actually drawn at (500ish px source down to ~20-30px on screen).
void bleedTransparentEdges(unsigned char* pixels, int w, int h) {
    std::vector<unsigned char> hasColor(static_cast<size_t>(w) * h);
    for (int i = 0; i < w * h; ++i) hasColor[i] = pixels[i * 4 + 3] != 0;

    constexpr int kIterations = 16;
    for (int iter = 0; iter < kIterations; ++iter) {
        bool anyChanged = false;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = y * w + x;
                if (hasColor[idx]) continue;
                const int nx[4] = {x - 1, x + 1, x, x};
                const int ny[4] = {y, y, y - 1, y + 1};
                for (int n = 0; n < 4; ++n) {
                    if (nx[n] < 0 || nx[n] >= w || ny[n] < 0 || ny[n] >= h) continue;
                    int nidx = ny[n] * w + nx[n];
                    if (!hasColor[nidx]) continue;
                    pixels[idx * 4 + 0] = pixels[nidx * 4 + 0];
                    pixels[idx * 4 + 1] = pixels[nidx * 4 + 1];
                    pixels[idx * 4 + 2] = pixels[nidx * 4 + 2];
                    // Alpha (byte 3) is deliberately left alone -- still 0.
                    hasColor[idx] = 1;
                    anyChanged = true;
                    break;
                }
            }
        }
        if (!anyChanged) break;
    }
}

bool loadPngTexture(const unsigned char* data, size_t len,
                     unsigned int* outTex, int* outW, int* outH) {
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, static_cast<int>(len), outW, outH, &channels, 4);
    if (!pixels) return false;
    bleedTransparentEdges(pixels, *outW, *outH);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    // These are all rasterized at a fixed 256px and then drawn much smaller
    // (e.g. the 18px sidebar icons) -- plain GL_LINEAR minification with no
    // mipmap has nothing to average over that large a downscale and aliases/
    // shimmers ("too sharp"). Mipmapping fixes that; MAG_FILTER stays
    // GL_LINEAR since magnification (the 72px logo, etc.) never hits this.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // A mild negative bias pulls sampling toward the sharper end of the mip
    // chain than trilinear filtering would pick on its own -- these icons
    // are flat single-color glyphs with no fine detail to shimmer, so the
    // usual aliasing tradeoff for sharpness isn't really visible here.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -0.75f);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, *outW, *outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    generateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    *outTex = tex;
    return true;
}

const char* updatePhaseLabel(UpdatePhase phase) {
    switch (phase) {
        case UpdatePhase::CheckingManifest:  return "Checking for updates";
        case UpdatePhase::PreparingUpdater:  return "Preparing updater";
        case UpdatePhase::Error:             return "Update check failed";
        default:                             return "";
    }
}

// One clickable row: an invisible full-size button (drives click/hover
// feedback) with an icon (if any) and label text drawn on top of it, so the
// icon reads as genuinely inside the clickable area rather than floating
// beside it. `bgColor`, when non-null, tints the button itself (used for
// the two big accent-colored launch buttons); sidebar entries and About's
// link rows pass nullptr and rely on ImGui's default hover/active shading
// alone. `centerContent` centers the icon+text as one group (launch
// buttons); otherwise it's left-aligned after `leftPadding` (sidebar/link
// rows, matching an ordinary list-item look).
bool renderIconRow(const char* id, const char* label, unsigned int iconTexture,
                    float w, float h, const ImVec4* bgColor, bool disabled,
                    bool centerContent, float iconSize, float leftPadding, ImFont* font) {
    ImVec2 pos = ImGui::GetCursorScreenPos();

    if (bgColor) ImGui::PushStyleColor(ImGuiCol_Button, *bgColor);
    ImGui::BeginDisabled(disabled);
    bool clicked = ImGui::Button(id, ImVec2(w, h));
    ImGui::EndDisabled();
    if (bgColor) ImGui::PopStyleColor();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    float alpha = disabled ? 0.55f : 1.0f;
    ImU32 tint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, alpha));

    // Dear ImGui's fonts all share one atlas texture, so drawing text via a
    // specific ImFont* here (rather than the currently-pushed one) needs no
    // extra texture binding -- ImDrawList::AddText(font, ...) handles that
    // internally. Resolve to the active font when the caller doesn't need a
    // different one (nullptr means "use whatever's already pushed").
    ImFont* resolvedFont = font ? font : ImGui::GetFont();
    float resolvedFontSize = font ? font->FontSize : ImGui::GetFontSize();

    if (font) ImGui::PushFont(font);
    ImVec2 textSize = ImGui::CalcTextSize(label);
    if (font) ImGui::PopFont();

    float iconGap = 8.0f * g_uiScale;
    float contentWidth = (iconTexture ? iconSize + iconGap : 0.0f) + textSize.x;
    float startX = centerContent ? pos.x + (w - contentWidth) * 0.5f : pos.x + leftPadding;

    if (iconTexture) {
        float iconY = pos.y + (h - iconSize) * 0.5f;
        drawList->AddImage((void*)(intptr_t)iconTexture, ImVec2(startX, iconY),
                            ImVec2(startX + iconSize, iconY + iconSize),
                            ImVec2(0, 0), ImVec2(1, 1), tint);
        startX += iconSize + iconGap;
    }

    float textY = pos.y + (h - textSize.y) * 0.5f;
    drawList->AddText(resolvedFont, resolvedFontSize, ImVec2(startX, textY), tint, label);

    return clicked && !disabled;
}

// Renders a small alert banner (tinted rounded rect background) containing
// `text`, wrapped to `width`. More visible than plain colored text for
// surfacing update/launch errors. Caller has already positioned the cursor
// via SetCursorPos; returns the total height consumed so the caller can
// advance its own layout past it.
float renderErrorBanner(const char* text, float width, ImFont* font) {
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    float kPadding = 10.0f * g_uiScale;
    ImFont* resolvedFont = font ? font : ImGui::GetFont();
    float resolvedFontSize = font ? font->FontSize : ImGui::GetFontSize();
    float wrapWidth = width - kPadding * 2.0f;

    ImVec2 textSize = resolvedFont->CalcTextSizeA(resolvedFontSize, FLT_MAX, wrapWidth, text);
    float bannerHeight = textSize.y + kPadding * 2.0f;

    drawList->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + bannerHeight),
                             ImGui::GetColorU32(kErrorBannerBg), 6.0f * g_uiScale);
    drawList->AddText(resolvedFont, resolvedFontSize, ImVec2(pos.x + kPadding, pos.y + kPadding),
                       ImGui::GetColorU32(kErrorBannerText), text, nullptr, wrapWidth);

    return bannerHeight;
}

void renderSidebar(App& app, const AppSnapshot& snap, float sidebarHeight,
                    unsigned int homeIconTexture, unsigned int infoIconTexture,
                    unsigned int settingsIconTexture) {
    float sidebarWidth = kSidebarWidth * g_uiScale;
    float rowHeight = 36.0f * g_uiScale;
    float iconSize = 22.0f * g_uiScale;
    float leftPadding = 16.0f * g_uiScale;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, kSidebarBg);
    ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("##sidebar", ImVec2(sidebarWidth, sidebarHeight), false,
        ImGuiWindowFlags_NoScrollbar);

    ImGui::Dummy(ImVec2(0.0f, 8.0f * g_uiScale));

    bool homeSelected = snap.activeTab == Tab::Start;
    ImVec4 homeColor = homeSelected ? kSidebarItemSelected : kSidebarItemUnselected;
    if (renderIconRow("##nav_home", "Home", homeIconTexture, sidebarWidth, rowHeight,
                       &homeColor, false, false, iconSize, leftPadding, nullptr)) {
        app.setActiveTab(Tab::Start);
    }

    bool settingsSelected = snap.activeTab == Tab::Settings;
    ImVec4 settingsColor = settingsSelected ? kSidebarItemSelected : kSidebarItemUnselected;
    if (renderIconRow("##nav_settings", "Settings", settingsIconTexture, sidebarWidth, rowHeight,
                       &settingsColor, false, false, iconSize, leftPadding, nullptr)) {
        app.setActiveTab(Tab::Settings);
    }

    // Push "About" to the bottom of the sidebar.
    float remaining = sidebarHeight - ImGui::GetCursorPosY() - 44.0f * g_uiScale;
    if (remaining > 0.0f) ImGui::Dummy(ImVec2(0.0f, remaining));

    bool aboutSelected = snap.activeTab == Tab::About;
    ImVec4 aboutColor = aboutSelected ? kSidebarItemSelected : kSidebarItemUnselected;
    if (renderIconRow("##nav_about", "About", infoIconTexture, sidebarWidth, rowHeight,
                       &aboutColor, false, false, iconSize, leftPadding, nullptr)) {
        app.setActiveTab(Tab::About);
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Thin separator between the sidebar and the content area -- drawn
    // after the child ends, on the parent window's draw list, so it isn't
    // clipped to the child's own bounds.
    ImVec2 winPos = ImGui::GetWindowPos();
    ImGui::GetWindowDrawList()->AddLine(
        ImVec2(winPos.x + sidebarWidth, winPos.y),
        ImVec2(winPos.x + sidebarWidth, winPos.y + sidebarHeight),
        kSeparatorColor, 1.0f * g_uiScale);
}

// Clicking either launch button spawns a detached watcher process and
// closes this whole launcher almost immediately (see App::requestLaunch())
// -- there's no "running"/"stopping" state to reflect here anymore, since
// nothing about this process is still around to track it once that happens.
void renderLaunchButton(App& app, LaunchTarget target, const char* id, const char* label,
                         unsigned int iconTexture, float x, float y, float w, float h, ImFont* semiBold) {
    ImGui::SetCursorPos(ImVec2(x, y));
    if (renderIconRow(id, label, iconTexture, w, h, &kAccent, false, true, 28.0f * g_uiScale, 0.0f, semiBold)) {
        app.requestLaunch(target);
    }
}

void renderStartTab(App& app, const AppSnapshot& snap, float contentX, float contentY,
                     float contentW, float contentH, unsigned int logoTexture,
                     unsigned int playerIconTexture, unsigned int studioIconTexture, ImFont* semiBold) {
    const float logoSize = 72.0f * g_uiScale;
    const float pad24 = 24.0f * g_uiScale;
    const float pad48 = 48.0f * g_uiScale;
    const float pad16 = 16.0f * g_uiScale;
    ImGui::SetCursorPos(ImVec2(contentX + (contentW - logoSize) * 0.5f, contentY + 20.0f * g_uiScale));
    if (logoTexture) {
        ImGui::Image((void*)(intptr_t)logoTexture, ImVec2(logoSize, logoSize));
    }

    bool updating = snap.update.phase == UpdatePhase::CheckingManifest ||
                     snap.update.phase == UpdatePhase::PreparingUpdater;

    if (updating) {
        const char* label = updatePhaseLabel(snap.update.phase);
        ImGui::PushFont(semiBold);
        float textWidth = ImGui::CalcTextSize(label).x;
        ImGui::SetCursorPos(ImVec2(contentX + (contentW - textWidth) * 0.5f, contentY + logoSize + 32.0f * g_uiScale));
        ImGui::TextUnformatted(label);
        ImGui::PopFont();

        ImGui::SetCursorPos(ImVec2(contentX + pad24, contentY + logoSize + 64.0f * g_uiScale));
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, kAccent);
        ImGui::ProgressBar(static_cast<float>(snap.update.fraction), ImVec2(contentW - pad48, 0));
        ImGui::PopStyleColor();
    } else {
        const float buttonW = (contentW - pad48 - pad16) / 2.0f;
        const float buttonH = 48.0f * g_uiScale;
        float buttonY = contentY + logoSize + 32.0f * g_uiScale;

        // A manifest fetch/parse failure is non-fatal (design intent: "Start
        // tab still usable"). Surface it above the buttons rather than
        // blocking them.
        if (snap.update.phase == UpdatePhase::Error) {
            ImGui::SetCursorPos(ImVec2(contentX + pad24, buttonY));
            float bannerHeight = renderErrorBanner(snap.update.errorMessage.c_str(), contentW - pad48, nullptr);
            buttonY += bannerHeight + 12.0f * g_uiScale;
        }

        renderLaunchButton(app, LaunchTarget::Player, "##launch_player", "Launch Player", playerIconTexture,
            contentX + pad24, buttonY, buttonW, buttonH, semiBold);
        renderLaunchButton(app, LaunchTarget::Studio, "##launch_studio", "Launch Studio", studioIconTexture,
            contentX + pad24 + buttonW + pad16, buttonY, buttonW, buttonH, semiBold);
    }

    char footer[64];
    snprintf(footer, sizeof(footer), "TuxBlox v%s", kTuxBloxVersion);
    ImGui::SetCursorPos(ImVec2(contentX + pad24, contentY + contentH - 28.0f * g_uiScale));
    ImGui::TextDisabled("%s", footer);
}

void renderAboutTab(float contentX, float contentY, float contentW, float contentH,
                     unsigned int logoTexture, unsigned int globeIconTexture,
                     unsigned int docsIconTexture, unsigned int githubIconTexture,
                     unsigned int discordIconTexture, unsigned int privacyIconTexture,
                     ImFont* semiBold) {
    const float logoSize = 72.0f * g_uiScale;
    const float pad24 = 24.0f * g_uiScale;
    const float pad48 = 48.0f * g_uiScale;
    ImGui::SetCursorPos(ImVec2(contentX + (contentW - logoSize) * 0.5f, contentY + 20.0f * g_uiScale));
    if (logoTexture) {
        ImGui::Image((void*)(intptr_t)logoTexture, ImVec2(logoSize, logoSize));
    }

    ImGui::PushFont(semiBold);
    const char* title = "About TuxBlox";
    float titleWidth = ImGui::CalcTextSize(title).x;
    ImGui::SetCursorPos(ImVec2(contentX + (contentW - titleWidth) * 0.5f, contentY + logoSize + 24.0f * g_uiScale));
    ImGui::TextUnformatted(title);
    ImGui::PopFont();

    struct LinkRow { const char* id; const char* label; const char* url; unsigned int icon; };
    const LinkRow links[] = {
        {"##link_website", "Website",       "https://tuxblox.net",         globeIconTexture},
        {"##link_docs",    "Documentation", "https://tuxblox.net/docs",    docsIconTexture},
        {"##link_github",  "GitHub",        "https://tuxblox.net/github",  githubIconTexture},
        {"##link_discord", "Discord",       "https://tuxblox.net/discord", discordIconTexture},
        {"##link_privacy", "Privacy Policy", "https://tuxblox.net/privacy", privacyIconTexture},
    };

    float y = contentY + logoSize + 64.0f * g_uiScale;
    for (const auto& link : links) {
        ImGui::SetCursorPos(ImVec2(contentX + pad24, y));
        if (renderIconRow(link.id, link.label, link.icon, contentW - pad48, 30.0f * g_uiScale,
                           &kTransparent, false, false, 22.0f * g_uiScale, 8.0f * g_uiScale, nullptr)) {
            openUrl(link.url);
        }
        y += 36.0f * g_uiScale;
    }

    char versionFooter[64];
    snprintf(versionFooter, sizeof(versionFooter), "TuxBlox v%s", kTuxBloxVersion);
    ImGui::SetCursorPos(ImVec2(contentX + pad24, contentY + contentH - 46.0f * g_uiScale));
    ImGui::TextDisabled("%s", versionFooter);
    ImGui::SetCursorPos(ImVec2(contentX + pad24, contentY + contentH - 28.0f * g_uiScale));
    ImGui::TextDisabled("%s", "\xC2\xA9 2026 TuxBlox Project"); // "©" (U+00A9) UTF-8
}

// A pill-shaped on/off switch -- Dear ImGui has no built-in equivalent.
// Draws at the current cursor position (like renderIconRow) and advances
// past it via the InvisibleButton driving the click target. Returns true
// on the frame it's clicked; the caller (not this function) flips the
// backing bool and persists it, same division of responsibility as every
// other interactive row in this file.
bool renderToggleSwitch(const char* id, bool value) {
    float toggleWidth = kToggleWidth * g_uiScale;
    float toggleHeight = kToggleHeight * g_uiScale;

    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(toggleWidth, toggleHeight));
    bool clicked = ImGui::IsItemClicked();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    const ImVec4& bgColor = value ? kAccent : kSidebarItemUnselected;
    drawList->AddRectFilled(pos, ImVec2(pos.x + toggleWidth, pos.y + toggleHeight),
                             ImGui::GetColorU32(bgColor), toggleHeight * 0.5f);

    float knobRadius = toggleHeight * 0.5f - 3.0f * g_uiScale;
    float knobY = pos.y + toggleHeight * 0.5f;
    float knobX = value ? pos.x + toggleWidth - toggleHeight * 0.5f
                         : pos.x + toggleHeight * 0.5f;
    drawList->AddCircleFilled(ImVec2(knobX, knobY), knobRadius, IM_COL32(255, 255, 255, 255));

    return clicked;
}

// A destructive-action button requiring a second click within 5 seconds to
// actually fire, e.g. "Uninstall TuxBlox" -> "Click again to uninstall".
// `pending`/`deadlineMs` are the caller's per-button confirm state (see
// their declaration comment in ui.h); `busy` shows `busyLabel` and disables
// the button instead (e.g. while a background thread carries out the
// action from a previous confirm). Returns true on the frame the second,
// confirming click lands -- the caller is responsible for actually doing
// the thing.
bool renderDangerButton(const char* id, const char* restLabel, const char* confirmLabel,
                         const char* busyLabel, bool busy, bool& pending, uint32_t& deadlineMs,
                         ImVec2 size) {
    if (pending && SDL_GetTicks() >= deadlineMs) pending = false;

    std::string label = busy ? busyLabel : (pending ? confirmLabel : restLabel);
    label += "##";
    label += id;

    ImGui::PushStyleColor(ImGuiCol_Button, pending ? kDangerColorArmed : kDangerColor);
    ImGui::BeginDisabled(busy);
    bool confirmed = false;
    if (ImGui::Button(label.c_str(), size)) {
        if (pending) {
            pending = false;
            confirmed = true;
        } else {
            pending = true;
            deadlineMs = SDL_GetTicks() + 5000;
        }
    }
    ImGui::EndDisabled();
    ImGui::PopStyleColor();
    return confirmed;
}

void renderSettingsTab(App& app, const AppSnapshot& snap, float contentX, float contentY,
                        float contentW, float contentH, char* protonBuf, size_t protonBufSize,
                        char* globalBuf, size_t globalBufSize, bool& buffersInitialized,
                        bool& uninstallConfirmPending, uint32_t& uninstallConfirmDeadlineMs,
                        bool& wipePrefixConfirmPending, uint32_t& wipePrefixConfirmDeadlineMs,
                        ImFont* semiBold) {
    // Seeded once from whatever App loaded/last saved -- see the buffer
    // fields' declaration comment in ui.h for why this can't just re-seed
    // every frame (it would stomp in-progress edits).
    if (!buffersInitialized) {
        snprintf(protonBuf, protonBufSize, "%s", snap.settings.protonEnvVars.c_str());
        snprintf(globalBuf, globalBufSize, "%s", snap.settings.globalEnvVars.c_str());
        buffersInitialized = true;
    }

    // Settings is the one tab whose content can outgrow the window (env var
    // fields, future options, ...), so it alone scrolls -- in its own child
    // region rather than letting the outer ##launcher window scroll, which
    // would drag the sidebar and its own scrollbar along with it. Content
    // below is laid out in this child's local coordinates (origin at
    // contentX/contentY), not the outer window's.
    const float pad24 = 24.0f * g_uiScale;

    ImGui::SetCursorPos(ImVec2(contentX, contentY));
    ImGui::BeginChild("##settings_scroll", ImVec2(contentW, contentH), false);

    ImGui::SetCursorPos(ImVec2(pad24, 20.0f * g_uiScale));
    ImGui::PushFont(semiBold);
    ImGui::TextUnformatted("Settings");
    ImGui::PopFont();

    const float fieldWidth = contentW - 48.0f * g_uiScale;
    float y = 64.0f * g_uiScale;

    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::TextUnformatted("Update Channel");
    y += 22.0f * g_uiScale;
    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::SetNextItemWidth(200.0f * g_uiScale);
    {
        static const char* kChannels[] = {"stable", "canary", "dev"};
        int currentIndex = 0;
        for (int i = 0; i < 3; ++i) {
            if (snap.settings.channel == kChannels[i]) { currentIndex = i; break; }
        }
        if (ImGui::Combo("##update_channel", &currentIndex, kChannels, 3)) {
            Settings s = snap.settings;
            s.channel = kChannels[currentIndex];
            app.updateSettings(s);
        }
    }
    y += 48.0f * g_uiScale;

    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::TextUnformatted("Proton Environment Variables");
    y += 22.0f * g_uiScale;
    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::InputText("##proton_env_vars", protonBuf, protonBufSize);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Settings s = snap.settings;
        s.protonEnvVars = protonBuf;
        app.updateSettings(s);
    }
    y += 40.0f * g_uiScale;

    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::TextUnformatted("Global Environment Variables");
    y += 22.0f * g_uiScale;
    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::SetNextItemWidth(fieldWidth);
    ImGui::InputText("##global_env_vars", globalBuf, globalBufSize);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        Settings s = snap.settings;
        s.globalEnvVars = globalBuf;
        app.updateSettings(s);
    }
    y += 48.0f * g_uiScale;

    float toggleHeight = kToggleHeight * g_uiScale;
    ImGui::SetCursorPos(ImVec2(pad24, y));
    bool toggled = renderToggleSwitch("##send_crash_reports", snap.settings.sendCrashReports);
    ImGui::SameLine(0.0f, 10.0f * g_uiScale);
    ImVec2 labelPos = ImGui::GetCursorScreenPos();
    ImGui::SetCursorScreenPos(ImVec2(labelPos.x, labelPos.y + (toggleHeight - ImGui::GetTextLineHeight()) * 0.5f));
    ImGui::TextUnformatted("Send Crash Report Data");
    if (toggled) {
        Settings s = snap.settings;
        s.sendCrashReports = !s.sendCrashReports;
        app.updateSettings(s);
    }
    y += toggleHeight + 8.0f * g_uiScale;

    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.58f, 1.0f));
    ImGui::PushTextWrapPos(pad24 + fieldWidth);
    ImGui::TextUnformatted(
        "Crash reports include only the exit code, Roblox/Proton version, and basic system info, "
        "see our privacy policy at tuxblox.net/privacy");
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    y += 60.0f * g_uiScale; // clears the two-line note above; not measured exactly, same approximation as everywhere else in this tab

    ImGui::SetCursorPos(ImVec2(pad24, y));
    ImGui::TextUnformatted("Danger Zone");
    y += 22.0f * g_uiScale;

    ImVec2 dangerBtnSize(220.0f * g_uiScale, 32.0f * g_uiScale);

    ImGui::SetCursorPos(ImVec2(pad24, y));
    if (renderDangerButton("wipe_prefix_btn", "Wipe Prefix", "Click again to wipe prefix",
                            "Wiping...", snap.wipePrefix.inProgress,
                            wipePrefixConfirmPending, wipePrefixConfirmDeadlineMs, dangerBtnSize)) {
        app.requestWipePrefix();
    }
    y += dangerBtnSize.y + 8.0f * g_uiScale;

    if (!snap.wipePrefix.errorMessage.empty()) {
        ImGui::SetCursorPos(ImVec2(pad24, y));
        y += renderErrorBanner(snap.wipePrefix.errorMessage.c_str(), fieldWidth, nullptr) + 8.0f * g_uiScale;
    }

    ImGui::SetCursorPos(ImVec2(pad24, y));
    if (renderDangerButton("uninstall_btn", "Uninstall TuxBlox", "Click again to uninstall",
                            "Uninstalling...", snap.uninstall.inProgress,
                            uninstallConfirmPending, uninstallConfirmDeadlineMs, dangerBtnSize)) {
        app.requestUninstall();
    }
    y += dangerBtnSize.y + 8.0f * g_uiScale;

    if (!snap.uninstall.errorMessage.empty()) {
        ImGui::SetCursorPos(ImVec2(pad24, y));
        renderErrorBanner(snap.uninstall.errorMessage.c_str(), fieldWidth, nullptr);
    }

    ImGui::EndChild();
}

} // namespace

Ui::Ui() = default;
Ui::~Ui() { shutdown(); }

bool Ui::init() {
    // SDL2's X11 backend otherwise derives WM_CLASS from the on-disk binary
    // name; desktop environments match a pinned taskbar icon's running
    // window back to its .desktop entry via StartupWMClass == WM_CLASS, so
    // without a fixed value here that match fails and the pin falls back to
    // a blank icon once the window closes. Must match StartupWMClass in the
    // .desktop entry written by ensureDesktopIntegration(). setenv() with
    // overwrite=0 leaves an operator-provided value alone.
    setenv("SDL_VIDEO_X11_WMCLASS", "tuxblox-launcher", 0);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        return false;
    }

    // Every pixel size/position/font size in this file is authored against a
    // 1440p baseline (g_uiScale == 1.0 there); scale it by the desktop's
    // actual resolution relative to that baseline so the window and its
    // contents occupy the same proportion of the screen at any resolution,
    // rather than a fixed pixel count that reads tiny on 4K and oversized on
    // 1080p. Clamped so an unusual/multi-monitor display mode can't produce
    // a degenerate window.
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

    // Native window decorations (title bar, minimize/maximize/close, and
    // resize) -- no custom borderless chrome, so the window manager owns
    // moving/resizing it like any other application window.
    SDL_Window* window = SDL_CreateWindow("TuxBlox",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(760 * g_uiScale), static_cast<int>(480 * g_uiScale),
        SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_RESIZABLE);
    if (!window) return false;
    window_ = window;
    SDL_SetWindowMinimumSize(window, static_cast<int>(640 * g_uiScale), static_cast<int>(420 * g_uiScale));

    SDL_GLContext gl = SDL_GL_CreateContext(window);
    if (!gl) return false;
    glContext_ = gl;
    SDL_GL_MakeCurrent(window, gl);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    // No top-level WindowRounding -- the native frame is square-cornered
    // (or rounded by the compositor itself), so an ImGui-drawn rounded
    // panel filling that frame would just clip oddly against it.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 10.0f;
    style.PopupRounding = 10.0f;
    style.FrameRounding = 8.0f;
    style.GrabRounding = 8.0f;
    style.Colors[ImGuiCol_WindowBg] = ImVec4(28.0f / 255.0f, 28.0f / 255.0f, 33.0f / 255.0f, 1.0f);
    style.Colors[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    // Scales all of ImGui's own built-in metrics (padding, spacing,
    // scrollbar size, and the rounding values set above) by the same factor
    // as everything else in this file.
    style.ScaleAllSizes(g_uiScale);

    ImGui_ImplSDL2_InitForOpenGL(window, gl);
    ImGui_ImplOpenGL3_Init("#version 150");

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

    loadPngTexture(kTuxbloxLogoPng, kTuxbloxLogoPngLen, &logoTexture_, &logoWidth_, &logoHeight_);
    {
        int w = 0, h = 0;
        loadPngTexture(kRobloxPlayerIconPng, kRobloxPlayerIconPngLen, &playerIconTexture_, &w, &h);
        loadPngTexture(kRobloxStudioIconPng, kRobloxStudioIconPngLen, &studioIconTexture_, &w, &h);
        loadPngTexture(kIconHomePng, kIconHomePngLen, &homeIconTexture_, &w, &h);
        loadPngTexture(kIconInfoPng, kIconInfoPngLen, &infoIconTexture_, &w, &h);
        loadPngTexture(kIconGlobePng, kIconGlobePngLen, &globeIconTexture_, &w, &h);
        loadPngTexture(kIconDocsPng, kIconDocsPngLen, &docsIconTexture_, &w, &h);
        loadPngTexture(kIconGithubPng, kIconGithubPngLen, &githubIconTexture_, &w, &h);
        loadPngTexture(kIconDiscordPng, kIconDiscordPngLen, &discordIconTexture_, &w, &h);
        loadPngTexture(kIconSettingsPng, kIconSettingsPngLen, &settingsIconTexture_, &w, &h);
        loadPngTexture(kIconPrivacyPng, kIconPrivacyPngLen, &privacyIconTexture_, &w, &h);
    }

    // A dedicated icon-sized export (see FetchWindowIcon.cmake), not the
    // same svg used for the in-app logo above -- decoded straight to an
    // SDL_Surface rather than through loadPngTexture() since this never
    // needs to be a GL texture. Independent of logoTexture_'s own success.
    {
        int iconWidth = 0, iconHeight = 0, ch = 0;
        unsigned char* pixels = stbi_load_from_memory(
            kTuxbloxWindowIconPng, static_cast<int>(kTuxbloxWindowIconPngLen), &iconWidth, &iconHeight, &ch, 4);
        if (pixels) {
            SDL_Surface* iconSurface = SDL_CreateRGBSurfaceWithFormatFrom(
                pixels, iconWidth, iconHeight, 32, iconWidth * 4, SDL_PIXELFORMAT_RGBA32);
            if (iconSurface) {
                SDL_SetWindowIcon(window, iconSurface);
                SDL_FreeSurface(iconSurface);
            }
            stbi_image_free(pixels);
        }
    }

    return true;
}

void Ui::shutdown() {
    auto freeTex = [](unsigned int& tex) {
        if (tex) { GLuint t = tex; glDeleteTextures(1, &t); tex = 0; }
    };
    freeTex(logoTexture_);
    freeTex(playerIconTexture_);
    freeTex(studioIconTexture_);
    freeTex(homeIconTexture_);
    freeTex(infoIconTexture_);
    freeTex(globeIconTexture_);
    freeTex(docsIconTexture_);
    freeTex(githubIconTexture_);
    freeTex(discordIconTexture_);
    freeTex(settingsIconTexture_);
    freeTex(privacyIconTexture_);
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
            shouldClose_ = true;
        }
    }

    auto snap = app.snapshot();

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
    // NoScrollbar/NoScrollWithMouse: only Settings' own content can overflow
    // (see renderSettingsTab's child region), so it alone should scroll --
    // never the whole window, which would drag the sidebar along with it.
    ImGui::Begin("##launcher", nullptr,
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImGui::PushFont(fontRegular_);

    renderSidebar(app, snap, static_cast<float>(h), homeIconTexture_, infoIconTexture_, settingsIconTexture_);

    const float contentX = kSidebarWidth * g_uiScale;
    const float contentY = 0.0f;
    const float contentW = static_cast<float>(w) - contentX;
    const float contentH = static_cast<float>(h);

    if (snap.activeTab == Tab::Start) {
        renderStartTab(app, snap, contentX, contentY, contentW, contentH,
                        logoTexture_, playerIconTexture_, studioIconTexture_, fontSemiBold_);
    } else if (snap.activeTab == Tab::Settings) {
        renderSettingsTab(app, snap, contentX, contentY, contentW, contentH,
                           protonEnvVarsBuf_, sizeof(protonEnvVarsBuf_),
                           globalEnvVarsBuf_, sizeof(globalEnvVarsBuf_),
                           settingsBuffersInitialized_,
                           uninstallConfirmPending_, uninstallConfirmDeadlineMs_,
                           wipePrefixConfirmPending_, wipePrefixConfirmDeadlineMs_, fontSemiBold_);
    } else {
        renderAboutTab(contentX, contentY, contentW, contentH, logoTexture_,
                        globeIconTexture_, docsIconTexture_, githubIconTexture_, discordIconTexture_,
                        privacyIconTexture_, fontSemiBold_);
    }

    ImGui::PopFont();
    ImGui::End();

    ImGui::Render();
    glViewport(0, 0, drawW, drawH);
    glClearColor(0.10980392f, 0.10980392f, 0.12941176f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    SDL_GL_SwapWindow(window);

    // One-time startup notice -- containerWarning is never cleared in
    // AppSnapshot (see its declaration comment), so "already shown" is
    // tracked here in Ui's own per-session state instead.
    if (!snap.containerWarning.empty() && !containerWarningShown_) {
        containerWarningShown_ = true;
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, "Distrobox GPU Passthrough",
            snap.containerWarning.c_str(), window);
    }

    // App::requestLaunch() has just spawned a detached watcher process (see
    // its own doc comment) and wants this whole launcher to close now --
    // whatever happens to Player/Studio from here on, this GUI has no
    // further part in it.
    if (app.shouldQuit()) {
        shouldClose_ = true;
    }

    return !shouldClose_;
}

} // namespace tuxblox
