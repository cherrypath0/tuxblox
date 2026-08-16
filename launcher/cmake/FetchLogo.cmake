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

# The TuxBlox logo: launcher/assets/tuxblox.png, checked into the repo
# directly. This used to fetch a remote SVG at build time and rasterize it
# via rsvg-convert -- that pipeline was silently producing a broken,
# near-blank 352-byte PNG (observed for real: the Home/About tabs' logo
# rendered as invisible), and depended on both network access and an
# rsvg-convert install for every build. Using the local, known-good asset
# fixes that and drops rsvg-convert as a required build dependency entirely.
# The PNG is consumed two ways:
#   - generate_logo_asset: the plain PNG itself, embedded into the launcher
#     via Qt's resource system (resources/launcher.qrc) for launcher_ui_qt.
#   - generate_logo_header: a generated C header (via BinToHeader.cmake)
#     still needed by launcher_core/src/desktop_integration.cpp, which
#     writes a .desktop icon file from the embedded bytes directly.

set(LOGO_PNG_PATH "${CMAKE_SOURCE_DIR}/assets/tuxblox.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(LOGO_HEADER_PATH "${GENERATED_DIR}/tuxblox_logo_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_target(generate_logo_asset DEPENDS ${LOGO_PNG_PATH})

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
