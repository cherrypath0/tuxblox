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
        pending_ = false;
        revertTimer_.stop();
        setText(restLabel_);
        emit confirmed();
    } else {
        pending_ = true;
        setText(confirmLabel_);
        revertTimer_.start(confirmWindowMs_);
    }
}

void DangerButton::revertArm() {
    pending_ = false;
    setText(restLabel_);
}

void DangerButton::setBusy(bool busy) {
    setEnabled(!busy);
    if (busy) {
        revertTimer_.stop();
        pending_ = false;
        setText(busyLabel_);
    } else if (!pending_) {
        setText(restLabel_);
    }
}

void DangerButton::setConfirmWindowMsForTesting(int ms) {
    confirmWindowMs_ = ms;
}

} // namespace tuxblox
