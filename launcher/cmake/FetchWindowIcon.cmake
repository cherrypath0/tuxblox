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

# Downloads the pre-rendered TuxBlox window/taskbar icon PNG. No
# rasterization step needed (unlike FetchLogo.cmake's logo, this is
# already a PNG, and a dedicated icon-sized export rather than the same
# svg used for the big in-app logo) -- runs at build time so the compiled
# launcher bundles the icon without needing network access itself.
#
# The PNG is consumed two ways:
#   - generate_window_icon_asset: the plain PNG itself, embedded via Qt's
#     resource system (resources/launcher.qrc) for launcher_ui_qt.
#   - generate_window_icon_header: a generated C header (via
#     BinToHeader.cmake) still needed by src/ui.cpp's SDL_SetWindowIcon
#     call -- this is transitional and goes away once Task 15 deletes
#     ui.cpp (Task 12's Qt MainWindow already sets its window icon from
#     the :/branding/tuxblox_window_icon.png resource directly), but
#     until then ui.cpp is still a TuxBloxLauncher source, so it stays.
# Both targets depend on the same fetched PNG rather than each re-fetching
# it, to avoid a duplicate network round trip and a build race between two
# custom commands writing the same output file.

set(WINDOW_ICON_PNG_URL "https://assetdelivery.tuxblox.net/images/png/icon/tuxblox-medium.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(WINDOW_ICON_PNG_PATH "${GENERATED_DIR}/tuxblox_window_icon.png")
set(WINDOW_ICON_HEADER_PATH "${GENERATED_DIR}/tuxblox_window_icon_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${WINDOW_ICON_PNG_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${WINDOW_ICON_PNG_URL} -DDEST=${WINDOW_ICON_PNG_PATH}
            -DUSERAGENT=TuxBlox-Client/1.0
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMENT "Fetching TuxBlox window icon"
    VERBATIM
)

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
