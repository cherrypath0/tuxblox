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

// Third Parties
#include <adwaita.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

namespace tuxblox {

void applyLook() {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_PREFER_DARK);
    g_object_set(gtk_settings_get_default(), "gtk-font-name", "Adwaita Sans 11", nullptr);
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
