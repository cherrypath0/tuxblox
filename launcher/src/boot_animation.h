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

namespace tuxblox {

// The launcher's opening sequence, as plain numbers: the drawing code and the
// tests can agree on it without either knowing about GTK.
struct BootAnimationFrame {
    double logoOpacity = 0.0;
    double logoScale = 1.0;
    // The logo is drawn with its own tilt in the asset. The splash cancels it
    // first, so the opening beat shows the mark upright, then puts it back.
    double logoAngleDegrees = 0.0;
    // 0 = the logo is centred on its own; 1 = it sits to the left of the name.
    double slideProgress = 0.0;
    double textOpacity = 0.0;
    // 0 = the name is still hidden behind the logo; 1 = it is fully out.
    double textReveal = 0.0;
    bool done = false;
};

// How long the entrance takes. The overlay fades out after this, revealing the
// normal content underneath it; the animation itself does not stretch to cover
// that time.
inline constexpr double kBootAnimationDurationMs = 1500.0;

// The frame at `elapsedMs` after the first frame clock tick. Times outside the
// sequence clamp to its ends.
BootAnimationFrame bootAnimationAt(double elapsedMs);

// Where the final lockup sits on a `surfaceWidth`-wide surface, and where the
// logo and name start before the entrance. The logo starts centred; the name
// starts behind it, at the logo's left edge.
struct BootLockupLayout {
    double finalLogoX = 0.0;
    double finalTextX = 0.0;
    double startLogoX = 0.0;
    double startTextX = 0.0;
};

BootLockupLayout bootLockupLayout(double surfaceWidth, double logoSize, double textWidth, double gap);

} // namespace tuxblox
