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

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

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
    "@define-color sidebar_selected_color #1a1f29;\n"
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
    "@define-color success_bg_color #047857;\n"
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
    "@define-color sidebar_selected_color #e9ebef;\n"
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
    ".card, list.boxed-list, list.boxed-list-separate > row { box-shadow: 0 0 0 1px @card_shade_color; }\n"
    // The interface picks a grey out of the text colour for the page you are on; this is the website's own raised surface instead, so the sidebar reads as one family with the cards beside it
    "list.navigation-sidebar row:selected { background-color: @sidebar_selected_color; }\n";

// One channel of a colour, straightened out of the curve a screen applies to it, which is what the eye's reading of brightness is built from.
double linearChannel(int value) {
    const double part = value / 255.0;
    return part <= 0.03928 ? part / 12.92 : std::pow((part + 0.055) / 1.055, 2.4);
}

// -1 for anything that is not a six-digit hex colour, including the three-digit shorthand and the non-hex forms libadwaita also accepts.
double relativeLuminance(const std::string& hex) {
    if (hex.size() != 7 || hex[0] != '#') return -1.0;
    int channel[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        const std::string pair = hex.substr(1 + i * 2, 2);
        if (pair.find_first_not_of("0123456789abcdefABCDEF") != std::string::npos) return -1.0;
        channel[i] = static_cast<int>(std::strtol(pair.c_str(), nullptr, 16));
    }
    return 0.2126 * linearChannel(channel[0]) + 0.7152 * linearChannel(channel[1]) +
           0.0722 * linearChannel(channel[2]);
}

} // namespace

const char* darkPaletteCss() {
    return DarkPalette;
}

const char* lightPaletteCss() {
    return LightPalette;
}

const char* paletteStylesCss() {
    return PaletteStyles;
}

std::string paletteColour(const char* pCss, const char* pName) {
    if (pCss == nullptr || pName == nullptr) return "";
    // The trailing space keeps a name from matching one it is only the beginning of.
    const std::string needle = std::string("@define-color ") + pName + " ";
    const std::string css(pCss);
    const size_t at = css.find(needle);
    if (at == std::string::npos) return "";
    const size_t from = at + needle.size();
    const size_t to = css.find(';', from);
    if (to == std::string::npos) return "";
    return css.substr(from, to - from);
}

double contrastRatio(const std::string& hexA, const std::string& hexB) {
    const double a = relativeLuminance(hexA);
    const double b = relativeLuminance(hexB);
    if (a < 0.0 || b < 0.0) return 0.0;
    const double lighter = a > b ? a : b;
    const double darker = a > b ? b : a;
    return (lighter + 0.05) / (darker + 0.05);
}

} // namespace tuxblox
