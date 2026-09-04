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

    // The off state is only one step lighter than the row behind it, so it
    // gets a hairline outline to stay visible. The on state doesn't need one.
    painter.setPen(isChecked() ? QPen(Qt::NoPen) : QPen(QColor(theme::kDivider)));
    painter.setBrush(isChecked() ? QColor(theme::kButtonBrandBg) : QColor(theme::kBgElevated));
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    painter.drawRoundedRect(track, kHeight / 2.0, kHeight / 2.0);

    const int knobRadius = kHeight / 2 - 3;
    const int knobY = kHeight / 2;
    const int knobX = isChecked() ? kWidth - kHeight / 2 : kHeight / 2;
    painter.setBrush(isChecked() ? QColor(Qt::white) : QColor(theme::kTextMutedDim));
    painter.drawEllipse(QPoint(knobX, knobY), knobRadius, knobRadius);
}

} // namespace tuxblox
