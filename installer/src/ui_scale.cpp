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

#include "ui_scale.h"

namespace tuxblox {

namespace {

constexpr float kBaselineDpi = 96.0f;
constexpr float kBaselineHeight = 1440.0f;
constexpr float kMinScale = 0.75f;
constexpr float kMaxScale = 3.0f;

float clampScale(float scale) {
    if (scale < kMinScale) return kMinScale;
    if (scale > kMaxScale) return kMaxScale;
    return scale;
}

} // namespace

float computeUiScale(float dpi, int displayHeightPx) {
    if (dpi > 0.0f) return clampScale(dpi / kBaselineDpi);
    if (displayHeightPx > 0) {
        return clampScale(static_cast<float>(displayHeightPx) / kBaselineHeight);
    }
    return 1.0f;
}

} // namespace tuxblox
