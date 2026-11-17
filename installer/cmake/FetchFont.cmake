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

# Fetches the Inter OFL-1.1 license text and embeds it, so the installer can write it into the
# installed COPYRIGHT.txt (the launcher still ships Inter). No font is embedded any more.

# Kept in the repository: a build must not need the network, and a licence notice must not come from a moving target.
set(INTER_OFL_TXT_SRC "${CMAKE_SOURCE_DIR}/assets/Inter-OFL.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(INTER_OFL_TXT_PATH "${GENERATED_DIR}/Inter-OFL.txt")
set(INTER_OFL_HEADER_PATH "${GENERATED_DIR}/inter_ofl_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${INTER_OFL_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${INTER_OFL_TXT_SRC} ${INTER_OFL_TXT_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_OFL_TXT_PATH} -DOUTPUT=${INTER_OFL_HEADER_PATH}
            -DSYMBOL=kInterOflLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter OFL license text"
    VERBATIM
)

add_custom_target(generate_font_header DEPENDS ${INTER_OFL_HEADER_PATH})
