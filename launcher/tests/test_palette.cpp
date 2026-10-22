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

#include "palette.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>

using namespace tuxblox;

namespace {

// Every one of these is drawn with white text on it, so each has to clear the 4.5:1 floor for
// text that size. Warning is left out on purpose: the dark palette pairs it with dark text.
const char* const kWhiteOnColour[] = {"accent_bg_color", "success_bg_color", "error_bg_color",
                                      "destructive_bg_color"};

void checkWhiteTextPasses(const char* pCss, const char* pPaletteName) {
    for (const char* pName : kWhiteOnColour) {
        const std::string colour = paletteColour(pCss, pName);
        assert(!colour.empty());
        const double ratio = contrastRatio(colour, "#ffffff");
        if (ratio < 4.5) {
            std::printf("%s palette: %s is %s, white text measures %.2f:1, needs 4.5:1\n",
                        pPaletteName, pName, colour.c_str(), ratio);
            // assert() aborts without flushing, and a contrast failure is worthless if it cannot say which colour.
            std::fflush(stdout);
        }
        assert(ratio >= 4.5);
    }
}

void test_known_ratios() {
    assert(std::fabs(contrastRatio("#ffffff", "#000000") - 21.0) < 0.01);
    assert(std::fabs(contrastRatio("#ffffff", "#ffffff") - 1.0) < 0.01);
}

void test_rejects_bad_hex() {
    assert(contrastRatio("", "#ffffff") == 0.0);
    assert(contrastRatio("#fff", "#ffffff") == 0.0);
    assert(contrastRatio("zzzzzz", "#ffffff") == 0.0);
}

// Reads a colour this task does not change, so Step 4's "only the contrast test fails" holds.
void test_extracts_a_colour() {
    assert(paletteColour(darkPaletteCss(), "accent_bg_color") == "#0273ae");
}

void test_missing_colour_is_empty() {
    assert(paletteColour(darkPaletteCss(), "nope").empty());
}

// The row for the page you are on has to be visible against the sidebar behind it. Reaching for a
// shared surface colour here picks white on white in the light palette, and the selection vanishes.
void test_selected_sidebar_row_is_visible() {
    const char* const palettes[][2] = {{darkPaletteCss(), "dark"}, {lightPaletteCss(), "light"}};
    for (const auto& palette : palettes) {
        const std::string selected = paletteColour(palette[0], "sidebar_selected_color");
        const std::string background = paletteColour(palette[0], "sidebar_bg_color");
        assert(!selected.empty());
        assert(!background.empty());
        const double step = contrastRatio(selected, background);
        if (step < 1.1) {
            std::printf("%s palette: selected row %s against sidebar %s is only %.3f:1 apart\n",
                        palette[1], selected.c_str(), background.c_str(), step);
            std::fflush(stdout);
        }
        assert(step >= 1.1);
    }
}

void test_dark_button_colours_pass_contrast() {
    checkWhiteTextPasses(darkPaletteCss(), "dark");
}

void test_light_button_colours_pass_contrast() {
    checkWhiteTextPasses(lightPaletteCss(), "light");
}

} // namespace

int main() {
    test_known_ratios();
    test_rejects_bad_hex();
    test_extracts_a_colour();
    test_missing_colour_is_empty();
    test_selected_sidebar_row_is_visible();
    test_dark_button_colours_pass_contrast();
    test_light_button_colours_pass_contrast();
    return 0;
}
