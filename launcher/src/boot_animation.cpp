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

#include "boot_animation.h"

#include <algorithm>

namespace tuxblox {

namespace {

// The logo fades in on its own, first and centred. The pause before the move
// is what makes it read as two beats instead of one continuous slide.
constexpr double kLogoFadeMs = 500.0;
constexpr double kMoveStartMs = 640.0;
constexpr double kMoveMs = 720.0;

// The name starts a little after the logo has begun moving away, so it is
// uncovered by the logo rather than racing it.
constexpr double kTextStartMs = 700.0;
constexpr double kTextMs = 760.0;
constexpr double kTextFadeMs = 360.0;

// Matches the tilt already drawn into tuxblox.png. Cancelling it puts the mark
// upright for the first beat; the move lets it settle back into its own angle.
constexpr double kLogoStartAngleDegrees = -12.0;

double clamp01(double value) {
    return std::clamp(value, 0.0, 1.0);
}

// The CSS prototype used cubic-bezier(.22, 1, .36, 1); an ease-out cubic is
// the same shape for the length of these movements and has no lookup table.
double easeOutCubic(double value) {
    const double t = clamp01(value);
    const double inverse = 1.0 - t;
    return 1.0 - inverse * inverse * inverse;
}

double phase(double elapsedMs, double startMs, double durationMs) {
    return clamp01((elapsedMs - startMs) / durationMs);
}

double lerp(double from, double to, double amount) {
    return from + (to - from) * amount;
}

} // namespace

BootAnimationFrame bootAnimationAt(double elapsedMs) {
    const double logoIn = easeOutCubic(phase(elapsedMs, 0.0, kLogoFadeMs));
    const double move = easeOutCubic(phase(elapsedMs, kMoveStartMs, kMoveMs));
    const double textReveal = easeOutCubic(phase(elapsedMs, kTextStartMs, kTextMs));
    const double textFade = easeOutCubic(phase(elapsedMs, kTextStartMs, kTextFadeMs));

    BootAnimationFrame frame;
    frame.logoOpacity = logoIn;
    frame.logoScale = lerp(0.94, 1.0, logoIn);
    frame.logoAngleDegrees = lerp(kLogoStartAngleDegrees, 0.0, move);
    frame.slideProgress = move;
    frame.textOpacity = textFade;
    frame.textReveal = textReveal;
    frame.done = elapsedMs >= kBootAnimationDurationMs;
    return frame;
}

BootLockupLayout bootLockupLayout(double surfaceWidth, double logoSize, double textWidth, double gap) {
    BootLockupLayout layout;
    const double totalWidth = logoSize + gap + textWidth;

    // The final lockup is centred as one piece. Before the move the logo is
    // centred on its own, and the name waits at the same left edge so the
    // logo covers it until it starts to slide away.
    layout.finalLogoX = (surfaceWidth - totalWidth) / 2.0;
    layout.finalTextX = layout.finalLogoX + logoSize + gap;
    layout.startLogoX = (surfaceWidth - logoSize) / 2.0;
    layout.startTextX = layout.startLogoX;
    return layout;
}

} // namespace tuxblox
