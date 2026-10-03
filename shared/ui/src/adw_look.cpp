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

#include "adw_look.h"

#include <climits>
#include <cstdlib>
#include <fstream>
#include <string>
#include <unistd.h>

// Third Parties
#include <adwaita.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include "json.hpp"

namespace tuxblox {

namespace {

// The colours of tuxblox.net, so the launcher and the website read as one product rather than two
const char DarkPalette[] =
    "@define-color window_bg_color #0a0c11;\n"
    "@define-color window_fg_color #e9ecf1;\n"
    "@define-color view_bg_color #0e1117;\n"
    "@define-color view_fg_color #e9ecf1;\n"
    "@define-color headerbar_bg_color #0e1117;\n"
    "@define-color headerbar_fg_color #e9ecf1;\n"
    "@define-color headerbar_backdrop_color #0a0c11;\n"
    "@define-color sidebar_bg_color #0e1117;\n"
    "@define-color sidebar_fg_color #e9ecf1;\n"
    "@define-color sidebar_backdrop_color #0a0c11;\n"
    "@define-color sidebar_border_color #232833;\n"
    "@define-color secondary_sidebar_bg_color #0e1117;\n"
    "@define-color secondary_sidebar_fg_color #e9ecf1;\n"
    "@define-color secondary_sidebar_backdrop_color #0a0c11;\n"
    "@define-color secondary_sidebar_border_color #232833;\n"
    "@define-color card_bg_color #13171f;\n"
    "@define-color card_fg_color #e9ecf1;\n"
    "@define-color card_shade_color #232833;\n"
    "@define-color dialog_bg_color #13171f;\n"
    "@define-color dialog_fg_color #e9ecf1;\n"
    "@define-color popover_bg_color #1a1f29;\n"
    "@define-color popover_fg_color #e9ecf1;\n"
    "@define-color thumbnail_bg_color #1a1f29;\n"
    "@define-color thumbnail_fg_color #e9ecf1;\n"
    "@define-color accent_bg_color #0273ae;\n"
    "@define-color accent_fg_color #ffffff;\n"
    "@define-color success_bg_color #059669;\n"
    "@define-color success_fg_color #ffffff;\n"
    "@define-color warning_bg_color #f59e0b;\n"
    "@define-color warning_fg_color rgb(0 0 6 / 80%);\n"
    "@define-color error_bg_color #dc2626;\n"
    "@define-color error_fg_color #ffffff;\n"
    "@define-color destructive_bg_color #dc2626;\n"
    "@define-color destructive_fg_color #ffffff;\n";

const char LightPalette[] =
    "@define-color window_bg_color #f5f6f8;\n"
    "@define-color window_fg_color #15181d;\n"
    "@define-color view_bg_color #ffffff;\n"
    "@define-color view_fg_color #15181d;\n"
    "@define-color headerbar_bg_color #ffffff;\n"
    "@define-color headerbar_fg_color #15181d;\n"
    "@define-color headerbar_backdrop_color #f5f6f8;\n"
    "@define-color sidebar_bg_color #ffffff;\n"
    "@define-color sidebar_fg_color #15181d;\n"
    "@define-color sidebar_backdrop_color #f5f6f8;\n"
    "@define-color sidebar_border_color #e1e4e9;\n"
    "@define-color secondary_sidebar_bg_color #ffffff;\n"
    "@define-color secondary_sidebar_fg_color #15181d;\n"
    "@define-color secondary_sidebar_backdrop_color #f5f6f8;\n"
    "@define-color secondary_sidebar_border_color #e1e4e9;\n"
    "@define-color card_bg_color #ffffff;\n"
    "@define-color card_fg_color #15181d;\n"
    "@define-color card_shade_color #e1e4e9;\n"
    "@define-color dialog_bg_color #ffffff;\n"
    "@define-color dialog_fg_color #15181d;\n"
    "@define-color popover_bg_color #ffffff;\n"
    "@define-color popover_fg_color #15181d;\n"
    "@define-color thumbnail_bg_color #ffffff;\n"
    "@define-color thumbnail_fg_color #15181d;\n"
    "@define-color accent_bg_color #0273ae;\n"
    "@define-color accent_fg_color #ffffff;\n"
    "@define-color success_bg_color #047857;\n"
    "@define-color success_fg_color #ffffff;\n"
    "@define-color warning_bg_color #b45309;\n"
    "@define-color warning_fg_color #ffffff;\n"
    "@define-color error_bg_color #dc2626;\n"
    "@define-color error_fg_color #ffffff;\n"
    "@define-color destructive_bg_color #dc2626;\n"
    "@define-color destructive_fg_color #ffffff;\n";

// A flat hairline in place of libadwaita's drop shadow, the way the website draws the same surfaces
const char PaletteStyles[] =
    ".card, list.boxed-list, list.boxed-list-separate > row { box-shadow: 0 0 0 1px @card_shade_color; }\n";

// Grey is libadwaita's own colours, which is what TuxBlox looked like before the website palette existed
bool isKnownTheme(const std::string &theme) {
    return theme == "system" || theme == "dark" || theme == "grey" || theme == "light";
}

AdwColorScheme colourSchemeFor(const std::string &theme) {
    if (theme == "dark") return ADW_COLOR_SCHEME_FORCE_DARK;
    if (theme == "light") return ADW_COLOR_SCHEME_FORCE_LIGHT;
    if (theme == "grey") return ADW_COLOR_SCHEME_PREFER_DARK;
    return ADW_COLOR_SCHEME_DEFAULT;
}

GtkCssProvider *pPalette = nullptr;
std::string CurrentTheme = "system";

void loadPalette() {
    if (pPalette == nullptr) return;
    std::string css;
    if (CurrentTheme != "grey") {
        const bool dark = CurrentTheme == "dark" ||
                          (CurrentTheme == "system" && adw_style_manager_get_dark(adw_style_manager_get_default()));
        css = std::string(dark ? DarkPalette : LightPalette) + PaletteStyles;
    }
    gtk_css_provider_load_from_string(pPalette, css.c_str());
}

void onColourSchemeChanged(AdwStyleManager *, GParamSpec *, gpointer) {
    loadPalette();
}

// The same rule every TuxBlox program follows to find its install, so they all read one settings file
std::string installRootDir() {
    const char *pRoot = std::getenv("TUXBLOX_ROOT");
    if (pRoot != nullptr && pRoot[0] == '/') return pRoot;

    char self[PATH_MAX];
    const ssize_t length = ::readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (length > 0) {
        self[length] = '\0';
        const std::string path(self);
        const size_t slash = path.rfind('/');
        if (slash != std::string::npos) return path.substr(0, slash);
    }

    const char *pHome = std::getenv("HOME");
    if (pHome != nullptr && pHome[0] != '\0') return std::string(pHome) + "/.tuxblox";
    return "";
}

} // namespace

void applyLook() {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_PREFER_DARK);
    g_object_set(gtk_settings_get_default(), "gtk-font-name", "Adwaita Sans 11", nullptr);
}

void applyTheme(const std::string &theme) {
    CurrentTheme = isKnownTheme(theme) ? theme : "system";
    AdwStyleManager *pManager = adw_style_manager_get_default();
    adw_style_manager_set_color_scheme(pManager, colourSchemeFor(CurrentTheme));

    if (pPalette == nullptr) {
        pPalette = gtk_css_provider_new();
        // The desktop can be switched between light and dark while the window is open
        g_signal_connect(pManager, "notify::dark", G_CALLBACK(onColourSchemeChanged), nullptr);
        gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(pPalette),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    loadPalette();
}

std::string settingsTheme() {
    const std::string root = installRootDir();
    if (root.empty()) return "system";
    try {
        std::ifstream file(root + "/settings.json", std::ios::binary);
        if (!file) return "system";
        nlohmann::json settings;
        file >> settings;
        const std::string theme = settings.value("theme", std::string("system"));
        return isKnownTheme(theme) ? theme : "system";
    } catch (...) {
        // A missing, unreadable or malformed settings file is not a reason to refuse to draw a window
        return "system";
    }
}

GdkTexture *iconTexture(const unsigned char *pPng, size_t length, int pixelSize) {
    GBytes *pBytes = g_bytes_new_static(pPng, length);
    GInputStream *pStream = g_memory_input_stream_new_from_bytes(pBytes);
    GdkPixbuf *pFullSize = gdk_pixbuf_new_from_stream(pStream, nullptr, nullptr);
    g_object_unref(pStream);
    g_bytes_unref(pBytes);
    if (pFullSize == nullptr) return nullptr;

    // Twice the size it is drawn at, so it is already sharp on a high-resolution screen and halves cleanly on an ordinary one
    const int target = pixelSize * 2;
    const int width = gdk_pixbuf_get_width(pFullSize);
    const int height = gdk_pixbuf_get_height(pFullSize);
    GdkPixbuf *pSized = pFullSize;
    if (pixelSize > 0 && width > target && height > target) {
        pSized = gdk_pixbuf_scale_simple(pFullSize, target, target, GDK_INTERP_BILINEAR);
        g_object_unref(pFullSize);
        if (pSized == nullptr) return nullptr;
    }

    GdkTexture *pTexture = gdk_texture_new_for_pixbuf(pSized);
    g_object_unref(pSized);
    return pTexture;
}

GdkPaintable *logoPaintable(const unsigned char *pPng, size_t length, int pixelSize) {
    return GDK_PAINTABLE(iconTexture(pPng, length, pixelSize));
}

} // namespace tuxblox
