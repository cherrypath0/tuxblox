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
#include <QString>

namespace tuxblox::theme {

// Palette derived from tuxblox.net's own site theme (see the design spec's
// "Visual Design System" section) -- a deliberate two-color blue/orange
// accent split rather than one brand color, on a near-black navy background.
inline constexpr const char* kBg = "#0a0e14";
inline constexpr const char* kPanelBg = "#10151d";
inline constexpr const char* kSidebarBg = "#0d1117";
inline constexpr const char* kBorder = "rgba(255, 255, 255, 0.08)";
inline constexpr const char* kAccentBlue = "#2f8fef";
inline constexpr const char* kAccentOrange = "#f5a623";
inline constexpr const char* kTextPrimary = "#e6e9ef";
inline constexpr const char* kTextMuted = "#8b93a1";
inline constexpr const char* kDanger = "#7a2626";
inline constexpr const char* kDangerArmed = "#a83232";
inline constexpr const char* kErrorBannerBg = "#3a2626";
inline constexpr const char* kErrorBannerText = "#f2c6c6";

// The single QSS stylesheet applied to the whole QApplication in main.cpp
// (qApp->setStyleSheet(theme::stylesheet())) -- every widget task below
// relies on the object names / classes styled here rather than pushing its
// own inline QSS, so the whole app reads as one visual system.
QString stylesheet();

} // namespace tuxblox::theme
