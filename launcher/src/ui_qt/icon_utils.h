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

// Loads `resourcePath` and explicitly rasterizes it into a QIcon at every
// size in {16,24,32,48,64,128,256} via addPixmap(), rather than relying on
// the single-size QIcon(path) constructor. Window-manager/compositor icon
// lookups (X11 _NET_WM_ICON, and by extension XWayland-hosted windows under
// a Wayland session) commonly enumerate an icon's available (size, mode,
// state) combinations looking for ones they recognize; a QIcon backed by
// just one embedded resource size was observed to make Qt's xcb backend
// publish an empty _NET_WM_ICON property (present but with zero pixel
// data) instead of the actual image, even though the QIcon itself loads
// successfully in-process. Explicitly populating several concrete raster
// sizes up front sidesteps whatever's making the single-size case
// invisible to the window manager. Use for window/taskbar icons
// specifically (setWindowIcon calls); paddedIcon() above remains the right
// choice for button icons, which don't go through this WM-facing path.
QIcon multiSizeWindowIcon(const QString& resourcePath);

} // namespace tuxblox
