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

#include <algorithm>
#include <cstring>
#include <string>
#include <string_view>

// Third Parties
// The drawing library is compiled in here and nowhere else: a second file that asks for its implementation is a duplicate-symbol error at link time.
#define NANOSVG_IMPLEMENTATION
#include <nanosvg.h>
#define NANOSVGRAST_IMPLEMENTATION
#include <nanosvgrast.h>

namespace tuxblox {

bool rasteriseSvgMask(const char* pSvg, size_t length, int pixelSize, std::vector<unsigned char>& outRgba) {
    if (pSvg == nullptr || length == 0 || pixelSize <= 0) return false;

    // The parser writes into the markup it is handed, so the caller's bytes -- usually a constant compiled into the program -- are copied before it sees them.
    std::string source(pSvg, length);
    NSVGimage* pImage = nsvgParse(source.data(), "px", 96.0f);
    if (pImage == nullptr) return false;
    // A drawing with no shapes in it, which is what a truncated or empty file parses to, would come out a fully transparent square -- a gap on the screen rather than anything anybody could see was wrong.
    if (pImage->width <= 0.0f || pImage->height <= 0.0f || pImage->shapes == nullptr) {
        nsvgDelete(pImage);
        return false;
    }

    NSVGrasterizer* pRasterizer = nsvgCreateRasterizer();
    if (pRasterizer == nullptr) {
        nsvgDelete(pImage);
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(pixelSize) * pixelSize * 4, 0);
    // Contained rather than stretched, so a drawing that is taller than it is wide keeps its shape instead of being squashed into the square.
    const float scale = std::min(static_cast<float>(pixelSize) / pImage->width,
                                 static_cast<float>(pixelSize) / pImage->height);
    nsvgRasterize(pRasterizer, pImage, 0.0f, 0.0f, scale, pixels.data(), pixelSize, pixelSize, pixelSize * 4);
    nsvgDeleteRasterizer(pRasterizer);
    nsvgDelete(pImage);

    // The colour the drawing came out is thrown away with the rest -- only how opaque each dot is survives into the icon.
    for (size_t i = 0; i < pixels.size(); i += 4) {
        pixels[i + 0] = 0;
        pixels[i + 1] = 0;
        pixels[i + 2] = 0;
    }

    outRgba = std::move(pixels);
    return true;
}

int iconTargetPixels(int pixelSize, int scale) {
    if (pixelSize <= 0) return 0;
    // Four is the ceiling past which the extra dots cost more than anybody can see, and one the floor a screen can report when there is no screen at all.
    const int usable = std::min(std::max(scale, 1), 4);
    return pixelSize * usable;
}

bool looksLikeSvg(const unsigned char* pBytes, size_t length) {
    if (pBytes == nullptr || length == 0) return false;
    // A compressed picture can hold any run of bytes at all, so its own signature settles the question before anything is searched for.
    static const unsigned char pngSignature[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (length >= sizeof(pngSignature) && std::memcmp(pBytes, pngSignature, sizeof(pngSignature)) == 0) return false;
    // The whole file is searched rather than its opening, because a licence notice carried as a comment pushes the root element past any fixed window.
    const std::string_view markup(reinterpret_cast<const char*>(pBytes), length);
    return markup.find("<svg") != std::string_view::npos;
}

} // namespace tuxblox
