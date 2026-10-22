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

#include "svg_raster.h"

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
#include "asset_scale.h"
#include "asset_settings.h"
#include "asset_stop.h"
#include "asset_tuxblox_icon.h"

#include <cassert>
#include <cstdio>
#include <vector>

using namespace tuxblox;

namespace {

struct Icon {
    const char* pName;
    const unsigned char* pBytes;
    size_t length;
};

// Every asset drawn as a tinted symbolic. The logo is deliberately absent -- see below.
const Icon kSymbolics[] = {
    {"home", kAssetHome, kAssetHomeLen},
    {"info", kAssetInfo, kAssetInfoLen},
    {"settings", kAssetSettings, kAssetSettingsLen},
    {"fastflags", kAssetFastflags, kAssetFastflagsLen},
    {"download", kAssetDownload, kAssetDownloadLen},
    {"play", kAssetPlay, kAssetPlayLen},
    {"stop", kAssetStop, kAssetStopLen},
    {"docs", kAssetDocs, kAssetDocsLen},
    {"privacy", kAssetPrivacy, kAssetPrivacyLen},
    {"scale", kAssetScale, kAssetScaleLen},
    {"globe", kAssetGlobe, kAssetGlobeLen},
    {"github", kAssetGithub, kAssetGithubLen},
    {"discord", kAssetDiscord, kAssetDiscordLen},
    {"roblox-player", kAssetRobloxPlayer, kAssetRobloxPlayerLen},
    {"roblox-studio", kAssetRobloxStudio, kAssetRobloxStudioLen},
    {"roblox-rdd", kAssetRobloxRdd, kAssetRobloxRddLen},
};

void drawsSomethingAt(const Icon& icon, int size) {
    std::vector<unsigned char> out;
    if (!rasteriseSvgMask(reinterpret_cast<const char*>(icon.pBytes), icon.length, size, out)) {
        std::printf("%s did not rasterise at %dpx -- empty, or markup nanosvg cannot read\n", icon.pName, size);
        std::fflush(stdout);
        assert(false);
    }
    bool anyCoverage = false;
    for (size_t i = 3; i < out.size(); i += 4) {
        if (out[i] > 0) anyCoverage = true;
    }
    if (!anyCoverage) {
        std::printf("%s rasterised to nothing at %dpx -- it would draw as a blank space\n", icon.pName, size);
        std::fflush(stdout);
    }
    assert(anyCoverage);
}

void test_every_icon_rasterises_at_16() {
    for (const Icon& icon : kSymbolics) drawsSomethingAt(icon, 16);
}

// Four times the smallest size they ship at, which is what the sharpest screen asks for.
void test_every_icon_rasterises_at_64() {
    for (const Icon& icon : kSymbolics) drawsSomethingAt(icon, 64);
}

// The logo's embedded bytes are written straight to a .png on disk for the desktop icon, by both
// the launcher and the installer. Converting it would break that silently, with no build error.
void test_logo_is_still_a_png() {
    assert(kAssetTuxbloxIconLen > 4);
    assert(kAssetTuxbloxIcon[0] == 0x89);
    assert(kAssetTuxbloxIcon[1] == 0x50);
    assert(kAssetTuxbloxIcon[2] == 0x4E);
    assert(kAssetTuxbloxIcon[3] == 0x47);
}

} // namespace

int main() {
    test_every_icon_rasterises_at_16();
    test_every_icon_rasterises_at_64();
    test_logo_is_still_a_png();
    return 0;
}
