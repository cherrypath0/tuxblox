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

#include "ui_qt/update_popup.h"
#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>
#include <QTest>
#include <cassert>
#include <cstdio>
#include <optional>

int main(int argc, char** argv) {
    using namespace tuxblox;
    QApplication app(argc, argv);

    UpdatePopup popup;

    // No version set -> hidden.
    popup.setAvailableVersion(std::nullopt);
    assert(!popup.isVisible());

    // Version set -> visible, and re-settable.
    popup.setAvailableVersion(std::string("2.4.0"));
    assert(popup.isVisible());

    QSignalSpy updateSpy(&popup, &UpdatePopup::updateRequested);
    QSignalSpy dismissSpy(&popup, &UpdatePopup::dismissRequested);

    // Clicking the dismiss button fires dismissRequested(), not updateRequested().
    auto* dismissButton = popup.findChild<QPushButton*>("updatePopupDismiss");
    assert(dismissButton != nullptr);
    dismissButton->click();
    assert(dismissSpy.count() == 1);
    assert(updateSpy.count() == 0);

    // Clicking anywhere else on the popup body fires updateRequested().
    QTest::mouseClick(&popup, Qt::LeftButton, Qt::NoModifier, QPoint(2, 2));
    assert(updateSpy.count() == 1);
    assert(dismissSpy.count() == 1); // unchanged

    // Clearing the version back to nullopt hides it again.
    popup.setAvailableVersion(std::nullopt);
    assert(!popup.isVisible());

    printf("update_popup: all tests passed\n");
    return 0;
}
