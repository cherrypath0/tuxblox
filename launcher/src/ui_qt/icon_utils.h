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

#pragma once
#include <QIcon>
#include <QSize>
#include <QString>

namespace tuxblox {

// QPushButton's built-in icon/text layout has a small fixed internal gap
// that isn't controllable via Qt stylesheets. To add extra breathing room
// between an icon and its button's text, this loads `resourcePath`, scales
// it to `size x size`, and pads it with `gapPx` of transparent space on the
// right -- Qt then reserves that full padded width as "the icon", pushing
// the text further right. Pair with iconSizeWithGap() in the matching
// setIconSize() call so the padded box isn't cropped.
QIcon paddedIcon(const QString& resourcePath, int size, int gapPx);
QSize iconSizeWithGap(int size, int gapPx);

} // namespace tuxblox
