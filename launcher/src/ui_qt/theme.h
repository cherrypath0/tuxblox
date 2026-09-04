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

#pragma once
#include <QFont>
#include <QString>

namespace tuxblox::theme {

// The launcher's palette is tuxblox.net's palette. Every value below is the
// site's own token of the same name, copied from its stylesheet so the two
// stay one visual system -- change them here only when the site changes.

// Surfaces. Near-black with a cool cast: the hue stays around 220 but the
// saturation is low, so the window reads dark rather than navy. The
// saturated blue is spent on the accents instead.
inline constexpr const char* kBg = "#0a0c11";
inline constexpr const char* kBgAlt = "#0e1117";
inline constexpr const char* kBgSoft = "#13171f";
inline constexpr const char* kBgElevated = "#1a1f29";
inline constexpr const char* kDivider = "#232833";
inline constexpr const char* kDividerSoft = "#1b2029";

// Brand.
inline constexpr const char* kBrand = "#38bdf8";
inline constexpr const char* kBrandSoft = "rgba(56, 189, 248, 0.12)";
inline constexpr const char* kAccent = "#f59e0b";

// Text. All three clear WCAG AA against every surface that carries text.
// Lighten kTextMutedDim before darkening it -- going dimmer fails AA.
inline constexpr const char* kTextPrimary = "#e9ecf1";
inline constexpr const char* kTextMuted = "#98a1b0";
inline constexpr const char* kTextMutedDim = "#79828f";

// Buttons. kButtonBrandBg is deliberately darker than kBrand: white text on
// kBrand is only 4.10:1, which fails AA at button text sizes.
inline constexpr const char* kButtonBrandBg = "#0273ae";
inline constexpr const char* kButtonBrandHover = "#01608f";
inline constexpr const char* kButtonAltBg = "#1a1f29";
inline constexpr const char* kButtonAltHover = "#232833";

// Destructive actions and error banners. The site has no equivalent, so
// these are tuned to sit beside it rather than copied from it.
inline constexpr const char* kDangerBorder = "#5c2626";
inline constexpr const char* kDangerText = "#e88f8f";
inline constexpr const char* kDangerArmedBg = "#7f2a2a";
inline constexpr const char* kErrorBannerBg = "rgba(190, 60, 60, 0.12)";
inline constexpr const char* kSuccess = "#22c55e";

// Radii, in pixels.
inline constexpr int kRadiusButton = 4;
inline constexpr int kRadiusCard = 6;

// Records the family names Qt actually assigned the embedded TTFs. Call
// once from main.cpp after registering the fonts and before stylesheet()
// -- fontsource's internal name metadata isn't guaranteed to be the literal
// "Inter"/"Montserrat", and the stylesheet has to name the real family.
void setFontFamilies(const QString& sans, const QString& display);

// Inter, the body face -- everything that isn't a heading. Montserrat, the
// display face, is reached through displayFont() rather than by name.
QString sansFamily();

// Montserrat at the given pixel size and weight. Qt stylesheets have no
// letter-spacing property, so headings have to set their font in code to
// get the site's tight display tracking.
QFont displayFont(int pixelSize, int weight);

// Inter, small and widely tracked, for the uppercase labels that head each
// group of settings. Same reason as displayFont() -- letter spacing.
QFont sectionLabelFont();

// The single QSS stylesheet applied to the whole QApplication in main.cpp.
// Every widget relies on the object names styled here rather than pushing
// its own inline QSS, so the whole app reads as one visual system.
QString stylesheet();

} // namespace tuxblox::theme
