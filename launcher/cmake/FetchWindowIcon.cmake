# TuxBlox - Linux Compatibility Layer for the Roblox Engine
# Copyright (C) 2026 TuxBlox Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# The TuxBlox window/taskbar icon: the same local asset as the logo
# (launcher/assets/tuxblox.png), checked into the repo directly. This used
# to fetch a separate pre-rendered PNG from a remote server at build time --
# using the same local asset for both removes a second network dependency
# and guarantees the window icon and the in-app logo are pixel-identical.
# The PNG is consumed two ways:
#   - generate_window_icon_asset: the plain PNG itself, embedded via Qt's
#     resource system (resources/launcher.qrc) for launcher_ui_qt (consumed
#     by MainWindow's setWindowIcon() and QApplication::setWindowIcon()).
#   - generate_window_icon_header: a generated C header (via
#     BinToHeader.cmake) still needed by desktop_integration.cpp, which
#     writes the same bytes to the XDG icon-theme directory directly.

set(WINDOW_ICON_PNG_PATH "${CMAKE_SOURCE_DIR}/assets/tuxblox.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(WINDOW_ICON_HEADER_PATH "${GENERATED_DIR}/tuxblox_window_icon_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_target(generate_window_icon_asset DEPENDS ${WINDOW_ICON_PNG_PATH})

add_custom_command(
    OUTPUT ${WINDOW_ICON_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${WINDOW_ICON_PNG_PATH} -DOUTPUT=${WINDOW_ICON_HEADER_PATH}
            -DSYMBOL=kTuxbloxWindowIconPng
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${WINDOW_ICON_PNG_PATH}
    COMMENT "Embedding TuxBlox window icon"
    VERBATIM
)

add_custom_target(generate_window_icon_header DEPENDS ${WINDOW_ICON_HEADER_PATH})
