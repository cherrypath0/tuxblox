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

# The TuxBlox window and taskbar icon: the same rendered file as the logo, so the two are
# pixel-identical and neither is fetched from a server at build time. See FetchLogo.cmake for why
# the rendering step has to pass an explicit size.
# Embedded as a C header, for desktop_integration.cpp, which writes it to the XDG icon theme directory.

if(NOT DEFINED TUXBLOX_ASSETS_BUILD)
    set(TUXBLOX_ASSETS_BUILD "${CMAKE_SOURCE_DIR}/../build/assets")
endif()
# Rendered from assets/tuxblox.svg by `make assets`.
set(WINDOW_ICON_PNG_PATH "${TUXBLOX_ASSETS_BUILD}/tuxblox/512x512.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(WINDOW_ICON_HEADER_PATH "${GENERATED_DIR}/tuxblox_window_icon_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

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
