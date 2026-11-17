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

# The TuxBlox logo, rendered from assets/tuxblox.svg by `make assets`.
#
# This once fetched a remote SVG at build time and rasterized it here, and that pipeline silently
# produced a broken, near-blank 352-byte PNG: the Home and About tabs' logo rendered as invisible.
# What makes rendering safe now is that the SVG is checked in rather than fetched, and that the
# `make assets` rule passes an explicit -w and -h. That SVG declares width="100%" height="100%",
# so a rasterizer left to guess its size is where a near-blank PNG comes from. Do not drop those
# flags, and do not point this at an SVG the build downloads.
# Embedded as a C header, for desktop_integration.cpp's icon file and the launcher window's logo.

if(NOT DEFINED TUXBLOX_ASSETS_BUILD)
    set(TUXBLOX_ASSETS_BUILD "${CMAKE_SOURCE_DIR}/../build/assets")
endif()
# Rendered from assets/tuxblox.svg by `make assets`. A raster, not the SVG: this one is written to
# disk as the desktop entry's icon file, which has to be a real PNG.
set(LOGO_PNG_PATH "${TUXBLOX_ASSETS_BUILD}/tuxblox/512x512.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(LOGO_HEADER_PATH "${GENERATED_DIR}/tuxblox_logo_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${LOGO_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${LOGO_PNG_PATH} -DOUTPUT=${LOGO_HEADER_PATH}
            -DSYMBOL=kTuxbloxLogoPng
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${LOGO_PNG_PATH}
    COMMENT "Embedding TuxBlox logo"
    VERBATIM
)

add_custom_target(generate_logo_header DEPENDS ${LOGO_HEADER_PATH})
