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

#include "ui_qt/toggle_switch.h"
#include <QApplication>
#include <QSignalSpy>
#include <cassert>
#include <cstdio>

int main(int argc, char** argv) {
    using namespace tuxblox;
    QApplication app(argc, argv);

    ToggleSwitch toggle;
    assert(toggle.isChecked() == false);

    toggle.setChecked(true);
    assert(toggle.isChecked() == true);

    QSignalSpy spy(&toggle, &ToggleSwitch::toggled);
    toggle.click();
    assert(spy.count() == 1);
    assert(toggle.isChecked() == false); // click() flips a checkable QAbstractButton
    assert(spy.at(0).at(0).toBool() == false);

    printf("toggle_switch: all tests passed\n");
    return 0;
}
