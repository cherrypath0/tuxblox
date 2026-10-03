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
#include "asset_discord.h"
#include "asset_docs.h"
#include "asset_download.h"
#include "asset_fastflags.h"
#include "asset_github.h"
#include "asset_globe.h"
#include "asset_home.h"
#include "asset_info.h"
#include "asset_play.h"
#include "asset_privacy.h"
#include "asset_roblox_player.h"
#include "asset_roblox_rdd.h"
#include "asset_roblox_studio.h"
#include "asset_settings.h"
#include "asset_stop.h"
#include "asset_tuxblox_icon.h"

#include <cassert>
#include <cstdio>

int main() {
    struct Asset {
        const char *pName;
        const unsigned char *pPng;
        size_t length;
    };
    const Asset assets[] = {
        {"discord", kAssetDiscord, kAssetDiscordLen},
        {"docs", kAssetDocs, kAssetDocsLen},
        {"download", kAssetDownload, kAssetDownloadLen},
        {"fastflags", kAssetFastflags, kAssetFastflagsLen},
        {"github", kAssetGithub, kAssetGithubLen},
        {"globe", kAssetGlobe, kAssetGlobeLen},
        {"home", kAssetHome, kAssetHomeLen},
        {"info", kAssetInfo, kAssetInfoLen},
        {"play", kAssetPlay, kAssetPlayLen},
        {"privacy", kAssetPrivacy, kAssetPrivacyLen},
        {"roblox-player", kAssetRobloxPlayer, kAssetRobloxPlayerLen},
        {"roblox-rdd", kAssetRobloxRdd, kAssetRobloxRddLen},
        {"roblox-studio", kAssetRobloxStudio, kAssetRobloxStudioLen},
        {"settings", kAssetSettings, kAssetSettingsLen},
        {"stop", kAssetStop, kAssetStopLen},
        {"tuxblox-icon", kAssetTuxbloxIcon, kAssetTuxbloxIconLen},
    };

    for (const Asset &asset : assets) {
        GdkPaintable *pIcon = tuxblox::symbolicIcon(asset.pPng, asset.length, 16);
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
