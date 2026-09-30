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

# Fetches the Inter OFL-1.1 license text and embeds it, for the COPYRIGHT.txt entry that Adwaita Sans refers to. No font is embedded any more.

set(INTER_OFL_URL "https://raw.githubusercontent.com/google/fonts/main/ofl/inter/OFL.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(INTER_OFL_TXT_PATH "${GENERATED_DIR}/Inter-OFL.txt")
set(INTER_OFL_HEADER_PATH "${GENERATED_DIR}/inter_ofl_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${INTER_OFL_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_OFL_URL} -DDEST=${INTER_OFL_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_OFL_TXT_PATH} -DOUTPUT=${INTER_OFL_HEADER_PATH}
            -DSYMBOL=kInterOflLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter OFL license text"
    VERBATIM
)

set(MONTSERRAT_OFL_URL "https://raw.githubusercontent.com/google/fonts/main/ofl/montserrat/OFL.txt")
set(MONTSERRAT_OFL_TXT_PATH "${GENERATED_DIR}/Montserrat-OFL.txt")
set(MONTSERRAT_OFL_HEADER_PATH "${GENERATED_DIR}/montserrat_ofl_license_txt.h")
add_custom_command(
    OUTPUT ${MONTSERRAT_OFL_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${MONTSERRAT_OFL_URL} -DDEST=${MONTSERRAT_OFL_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${MONTSERRAT_OFL_TXT_PATH} -DOUTPUT=${MONTSERRAT_OFL_HEADER_PATH}
            -DSYMBOL=kMontserratOflLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Montserrat OFL license text"
    VERBATIM
)

add_custom_target(generate_font_header DEPENDS ${INTER_OFL_HEADER_PATH} ${MONTSERRAT_OFL_HEADER_PATH})
