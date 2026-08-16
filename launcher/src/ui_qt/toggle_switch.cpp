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

#include "toggle_switch.h"
#include "theme.h"
#include <QPainter>

namespace tuxblox {

ToggleSwitch::ToggleSwitch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
}

QSize ToggleSwitch::sizeHint() const {
    return QSize(kWidth, kHeight);
}

void ToggleSwitch::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor bg = isChecked() ? QColor(theme::kAccentBlue) : QColor(255, 255, 255, 60);
    painter.setPen(Qt::NoPen);
    painter.setBrush(bg);
    painter.drawRoundedRect(rect(), kHeight / 2.0, kHeight / 2.0);

    const int knobRadius = kHeight / 2 - 3;
    const int knobY = kHeight / 2;
    const int knobX = isChecked() ? kWidth - kHeight / 2 : kHeight / 2;
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(knobX, knobY), knobRadius, knobRadius);
}

} // namespace tuxblox
