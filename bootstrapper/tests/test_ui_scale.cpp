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
#include <cassert>
#include <cmath>
#include <cstdio>

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.001f; }
} // namespace

int main() {
    using namespace tuxblox;

    // The point of the change: one physical size everywhere. Two displays at
    // the same DPI must agree however many pixels tall they are.
    assert(near(computeUiScale(96.0f, 1080), computeUiScale(96.0f, 2160)));
    assert(near(computeUiScale(144.0f, 1440), computeUiScale(144.0f, 2160)));

    // 96 DPI is the desktop convention for 100%.
    assert(near(computeUiScale(96.0f, 1440), 1.0f));
    assert(near(computeUiScale(192.0f, 2160), 2.0f));
    assert(near(computeUiScale(144.0f, 2160), 1.5f));

    // No DPI from the platform (common under Wayland): fall back to the old
    // 1440p-baseline resolution ratio rather than guessing 1.0.
    assert(near(computeUiScale(0.0f, 1440), 1.0f));
    assert(near(computeUiScale(0.0f, 2160), 1.5f));
    assert(near(computeUiScale(-1.0f, 2160), 1.5f));

    // Clamped at both ends, whichever path produced the value.
    assert(near(computeUiScale(24.0f, 1440), 0.75f));
    assert(near(computeUiScale(600.0f, 1440), 3.0f));
    assert(near(computeUiScale(0.0f, 480), 0.75f));
    assert(near(computeUiScale(0.0f, 8640), 3.0f));

    // Nothing usable at all still gives a sane window.
    assert(near(computeUiScale(0.0f, 0), 1.0f));

    printf("ui_scale: all tests passed\n");
    return 0;
}
