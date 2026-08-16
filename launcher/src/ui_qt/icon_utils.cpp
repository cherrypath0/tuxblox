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

#include "icon_utils.h"
#include <QPainter>
#include <QPixmap>

namespace tuxblox {

QIcon paddedIcon(const QString& resourcePath, int size, int gapPx) {
    QPixmap source = QIcon(resourcePath).pixmap(size, size);
    QPixmap padded(size + gapPx, size);
    padded.fill(Qt::transparent);
    QPainter painter(&padded);
    painter.drawPixmap(0, 0, source);
    painter.end();
    return QIcon(padded);
}

QSize iconSizeWithGap(int size, int gapPx) {
    return QSize(size + gapPx, size);
}

} // namespace tuxblox
