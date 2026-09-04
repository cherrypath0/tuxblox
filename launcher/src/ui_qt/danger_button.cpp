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

#include "danger_button.h"
#include <QStyle>

namespace tuxblox {

DangerButton::DangerButton(const QString& restLabel, const QString& confirmLabel,
                            const QString& busyLabel, QWidget* parent)
    : QPushButton(restLabel, parent),
      restLabel_(restLabel),
      confirmLabel_(confirmLabel),
      busyLabel_(busyLabel) {
    setObjectName("dangerButton");
    revertTimer_.setSingleShot(true);
    connect(&revertTimer_, &QTimer::timeout, this, &DangerButton::revertArm);
    connect(this, &QPushButton::clicked, this, &DangerButton::handleClicked);
}

void DangerButton::handleClicked() {
    if (pending_) {
        setArmed(false);
        revertTimer_.stop();
        emit confirmed();
    } else {
        setArmed(true);
        revertTimer_.start(confirmWindowMs_);
    }
}

void DangerButton::revertArm() {
    setArmed(false);
}

// Armed is a filled red button rather than an outlined one, so the second
// click is visibly the destructive one. The stylesheet selects on the
// property, which Qt only re-evaluates when the widget is repolished.
void DangerButton::setArmed(bool armed) {
    pending_ = armed;
    setText(armed ? confirmLabel_ : restLabel_);
    setProperty("armed", armed);
    style()->unpolish(this);
    style()->polish(this);
}

void DangerButton::setBusy(bool busy) {
    setEnabled(!busy);
    if (busy) {
        revertTimer_.stop();
        setArmed(false);
        setText(busyLabel_);
    } else if (!pending_) {
        setText(restLabel_);
    }
}

void DangerButton::setConfirmWindowMsForTesting(int ms) {
    confirmWindowMs_ = ms;
}

} // namespace tuxblox
