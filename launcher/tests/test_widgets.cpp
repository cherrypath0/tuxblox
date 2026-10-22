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

#include "widgets.h"
#include "asset_table.h"

#include <cassert>
#include <cstdio>

int main() {
    // Every asset the build embeds, from EmbedAssets.cmake, so a new icon is covered here the moment it is added there.

    for (size_t index = 0; index < kEmbeddedAssetCount; ++index) {
        const EmbeddedAsset &asset = kEmbeddedAssets[index];
        GdkPaintable *pIcon = tuxblox::symbolicIcon(asset.pBytes, asset.length, 16);
        // The PNGs are white, so anything but a symbolic paintable disappears in light mode
        assert(GTK_IS_SYMBOLIC_PAINTABLE(pIcon));
        const int width = gdk_paintable_get_intrinsic_width(pIcon);
        const int height = gdk_paintable_get_intrinsic_height(pIcon);
        // Resized to twice the size it is drawn at, so the renderer is never left shrinking a 96 px image into a 16 px box
        if (width <= 0 || width > 32 || height <= 0 || height > 32) {
            fprintf(stderr, "%s decodes to %dx%d, expected 1-32 px each way\n", asset.pName, width, height);
            return 1;
        }
        g_object_unref(pIcon);
    }

    const unsigned char notAPng[] = {1, 2, 3, 4};
    GdkPaintable *pBroken = tuxblox::symbolicIcon(notAPng, sizeof(notAPng), 16);
    assert(pBroken != nullptr);
    assert(gdk_paintable_get_intrinsic_width(pBroken) == 0);
    g_object_unref(pBroken);

    printf("widgets: all tests passed\n");
    return 0;
}
