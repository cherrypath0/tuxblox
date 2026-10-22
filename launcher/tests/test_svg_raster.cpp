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

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

using namespace tuxblox;

namespace {

// A 24x24 viewBox filled edge to edge.
const char kFilled[] = "<svg viewBox=\"0 0 24 24\"><rect x=\"0\" y=\"0\" width=\"24\" height=\"24\"/></svg>";
const char kTallViewBox[] = "<svg viewBox=\"0 0 24 48\"><rect x=\"0\" y=\"0\" width=\"24\" height=\"48\"/></svg>";

size_t alphaAt(int size, int x, int y) {
    return static_cast<size_t>(y * size + x) * 4 + 3;
}

void test_produces_requested_size() {
    std::vector<unsigned char> out;
    assert(rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, 16, out));
    assert(out.size() == 16u * 16u * 4u);
}

void test_centre_is_opaque() {
    std::vector<unsigned char> out;
    assert(rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, 16, out));
    assert(out[alphaAt(16, 8, 8)] == 255);
}

void test_colour_is_discarded() {
    std::vector<unsigned char> out;
    assert(rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, 16, out));
    const size_t pixel = static_cast<size_t>(8 * 16 + 8) * 4;
    assert(out[pixel + 0] == 0);
    assert(out[pixel + 1] == 0);
    assert(out[pixel + 2] == 0);
}

void test_rejects_empty_input() {
    std::vector<unsigned char> out;
    assert(!rasteriseSvgMask("", 0, 16, out));
}

void test_rejects_malformed_input() {
    std::vector<unsigned char> out;
    const char unterminated[] = "<svg";
    assert(!rasteriseSvgMask(unterminated, sizeof(unterminated) - 1, 16, out));
}

void test_rejects_nonpositive_size() {
    // Pre-filled with a sentinel, so "left untouched" is something the test can see.
    std::vector<unsigned char> out(3, 0xAB);
    assert(!rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, 0, out));
    assert(out.size() == 3u && out[0] == 0xAB);
    assert(!rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, -1, out));
    assert(out.size() == 3u && out[0] == 0xAB);
}

// nsvgParse() writes into the buffer it is handed, so an implementation that does not copy first
// corrupts the embedded asset it was given and every later render of that icon comes back blank.
void test_does_not_mutate_caller_buffer() {
    const std::string before(kFilled, sizeof(kFilled) - 1);
    std::vector<unsigned char> out;
    assert(rasteriseSvgMask(kFilled, sizeof(kFilled) - 1, 16, out));
    assert(std::memcmp(before.data(), kFilled, sizeof(kFilled) - 1) == 0);
}

void test_non_square_viewbox_fills_height() {
    std::vector<unsigned char> out;
    assert(rasteriseSvgMask(kTallViewBox, sizeof(kTallViewBox) - 1, 16, out));
    bool bottomRowDrawn = false;
    for (int x = 0; x < 16; ++x) {
        if (out[alphaAt(16, x, 15)] > 0) bottomRowDrawn = true;
    }
    assert(bottomRowDrawn);
}

} // namespace

int main() {
    test_produces_requested_size();
    test_centre_is_opaque();
    test_colour_is_discarded();
    test_rejects_empty_input();
    test_rejects_malformed_input();
    test_rejects_nonpositive_size();
    test_does_not_mutate_caller_buffer();
    test_non_square_viewbox_fills_height();
    return 0;
}
