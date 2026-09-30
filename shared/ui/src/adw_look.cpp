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

namespace tuxblox {

void applyLook() {
    adw_style_manager_set_color_scheme(adw_style_manager_get_default(), ADW_COLOR_SCHEME_PREFER_DARK);
    g_object_set(gtk_settings_get_default(), "gtk-font-name", "Adwaita Sans 11", nullptr);
}

GdkPaintable *logoPaintable(const unsigned char *pPng, size_t length) {
    GBytes *pBytes = g_bytes_new_static(pPng, length);
    GdkTexture *pTexture = gdk_texture_new_from_bytes(pBytes, nullptr);
    g_bytes_unref(pBytes);
    return GDK_PAINTABLE(pTexture);
}

} // namespace tuxblox
