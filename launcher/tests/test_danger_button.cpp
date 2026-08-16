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

#include "ui_qt/danger_button.h"
#include <QApplication>
#include <QSignalSpy>
#include <QTest>
#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
    using namespace tuxblox;
    QApplication app(argc, argv);

    DangerButton button("Wipe Prefix", "Click again to wipe prefix", "Wiping...");
    assert(button.text() == "Wipe Prefix");

    QSignalSpy spy(&button, &DangerButton::confirmed);

    // First click arms it -- relabels, doesn't fire confirmed().
    button.click();
    assert(button.text() == "Click again to wipe prefix");
    assert(spy.count() == 0);

    // Second click within the window confirms.
    button.click();
    assert(spy.count() == 1);
    assert(button.text() == "Wipe Prefix"); // reverts immediately after confirming

    // Arm again, then let the 5s window lapse without a second click --
    // waiting the full 5s in a test is wasteful, so this exercises the
    // revert path with a short window instead via the testing-only setter.
    button.setConfirmWindowMsForTesting(50);
    button.click();
    assert(button.text() == "Click again to wipe prefix");
    QTest::qWait(100);
    assert(button.text() == "Wipe Prefix");
    assert(spy.count() == 1); // still just the one earlier confirm -- the lapsed arm never fired

    button.setBusy(true);
    assert(button.text() == "Wiping...");
    assert(!button.isEnabled());
    button.setBusy(false);
    assert(button.text() == "Wipe Prefix");
    assert(button.isEnabled());

    printf("danger_button: all tests passed\n");
    return 0;
}
