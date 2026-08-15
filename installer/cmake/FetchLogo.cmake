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

# Downloads the pre-rendered TuxBlox icon PNG and generates a C header
# embedding its bytes. Runs at build time so the compiled installer bundles
# the logo without needing network access itself -- only the build machine
# needs it.
#
# This is the same dedicated icon-sized export the launcher's window icon
# uses (see launcher/cmake/FetchWindowIcon.cmake), not the older
# rsvg-convert rasterization of images/svg/tuxblox.svg -- so the installer
# window, the .desktop icon it writes, and the launcher all show the same
# artwork. Because the asset ships as a PNG there is no rasterization step,
# which is also why the installer build no longer needs rsvg-convert.

set(LOGO_PNG_URL "https://static.tuxblox.net/images/png/icon/tuxblox-medium.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(LOGO_PNG_PATH "${GENERATED_DIR}/tuxblox_logo.png")
set(LOGO_HEADER_PATH "${GENERATED_DIR}/tuxblox_logo_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${LOGO_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${LOGO_PNG_URL} -DDEST=${LOGO_PNG_PATH}
            -DUSERAGENT=TuxBlox-Client/1.0
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${LOGO_PNG_PATH} -DOUTPUT=${LOGO_HEADER_PATH}
            -DSYMBOL=kTuxbloxLogoPng
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding TuxBlox logo"
    VERBATIM
)

add_custom_target(generate_logo_header DEPENDS ${LOGO_HEADER_PATH})
