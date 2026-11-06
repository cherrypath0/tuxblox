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

#include <cassert>
#include <cmath>

using namespace tuxblox;

namespace {

void close(double actual, double expected, double epsilon = 1e-9) {
    assert(std::fabs(actual - expected) <= epsilon);
}

void testEntrance() {
    // The first beat: the mark is already upright, centred and still hidden.
    const BootAnimationFrame start = bootAnimationAt(0.0);
    close(start.logoOpacity, 0.0);
    close(start.logoScale, 0.94);
    close(start.logoAngleDegrees, -12.0);
    close(start.slideProgress, 0.0);
    close(start.textOpacity, 0.0);
    close(start.textReveal, 0.0);
    assert(!start.done);

    // The pause: fully visible, still upright, nothing has moved yet.
    const BootAnimationFrame pause = bootAnimationAt(500.0);
    close(pause.logoOpacity, 1.0);
    close(pause.logoScale, 1.0);
    close(pause.logoAngleDegrees, -12.0);
    close(pause.slideProgress, 0.0);
    close(pause.textOpacity, 0.0);
    close(pause.textReveal, 0.0);

    // The move has started, but the name still has its short delay.
    const BootAnimationFrame movementStart = bootAnimationAt(640.0);
    close(movementStart.slideProgress, 0.0);
    close(movementStart.textReveal, 0.0);

    // Halfway through: the logo is turning and sliding, the name is coming out.
    const BootAnimationFrame middle = bootAnimationAt(1000.0);
    assert(middle.slideProgress > 0.0 && middle.slideProgress < 1.0);
    assert(middle.logoAngleDegrees > -12.0 && middle.logoAngleDegrees < 0.0);
    assert(middle.textOpacity > 0.0 && middle.textOpacity < 1.0);
    assert(middle.textReveal > 0.0 && middle.textReveal < 1.0);

    // The end: the lockup is in its final arrangement and stays there.
    const BootAnimationFrame end = bootAnimationAt(kBootAnimationDurationMs);
    close(end.logoOpacity, 1.0);
    close(end.logoScale, 1.0);
    close(end.logoAngleDegrees, 0.0);
    close(end.slideProgress, 1.0);
    close(end.textOpacity, 1.0);
    close(end.textReveal, 1.0);
    assert(end.done);

    const BootAnimationFrame after = bootAnimationAt(5000.0);
    close(after.slideProgress, 1.0);
    close(after.logoAngleDegrees, 0.0);
    close(after.textReveal, 1.0);
    assert(after.done);
}

void testMonotonic() {
    double lastSlide = -1.0;
    double lastAngle = -12.0;
    double lastReveal = -1.0;

    // Sampled finely: every value only ever moves towards its final state.
    for (int at = 0; at <= static_cast<int>(kBootAnimationDurationMs); at += 10) {
        const BootAnimationFrame frame = bootAnimationAt(at);
        assert(frame.slideProgress >= lastSlide);
        assert(frame.logoAngleDegrees >= lastAngle - 1e-9);
        assert(frame.textReveal >= lastReveal);
        lastSlide = frame.slideProgress;
        lastAngle = frame.logoAngleDegrees;
        lastReveal = frame.textReveal;
    }
}

void testLayout() {
    const double surface = 560.0;
    const double logo = 124.0;
    const double text = 230.0;
    const double gap = 22.0;

    const BootLockupLayout layout = bootLockupLayout(surface, logo, text, gap);

    // Before the move the logo is centred on its own, and the name waits at
    // the same left edge so the logo can cover it.
    close(layout.startLogoX, (surface - logo) / 2.0);
    close(layout.startTextX, layout.startLogoX);
    close(layout.startLogoX + logo / 2.0, surface / 2.0);

    // After the move the whole lockup is centred as one piece.
    const double totalWidth = logo + gap + text;
    close(layout.finalLogoX, (surface - totalWidth) / 2.0);
    close(layout.finalTextX, layout.finalLogoX + logo + gap);
    close(layout.finalLogoX + totalWidth + layout.finalLogoX, surface);

    // An odd surface width must not round the lockup off-centre.
    const BootLockupLayout odd = bootLockupLayout(561.0, logo, text, gap);
    close(odd.finalLogoX, (561.0 - totalWidth) / 2.0);
    close(odd.finalLogoX + totalWidth + odd.finalLogoX, 561.0);
}

} // namespace

int main() {
    testEntrance();
    testMonotonic();
    testLayout();
    return 0;
}
