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

# Downloads two static-weight Inter TTFs (OFL-1.1) from jsDelivr's fontsource
# mirror, pinned to a fixed release so a later cmake run can't silently swap
# in a different font, then generates C headers embedding the raw TTF bytes
# so the compiled installer needs no network access at runtime to show text.
#
# Google Fonts' own upstream repo only ships Inter as a single variable-weight
# TTF; stb_truetype (which ImGui uses to rasterize fonts) has no way to select
# a specific weight instance out of a variable font, so a pre-split static
# build is needed to get two genuinely different weights (Regular/SemiBold).

set(INTER_VERSION "5.0.0")
set(INTER_REGULAR_URL "https://cdn.jsdelivr.net/fontsource/fonts/inter@${INTER_VERSION}/latin-400-normal.ttf")
set(INTER_SEMIBOLD_URL "https://cdn.jsdelivr.net/fontsource/fonts/inter@${INTER_VERSION}/latin-600-normal.ttf")
set(INTER_OFL_URL "https://raw.githubusercontent.com/google/fonts/main/ofl/inter/OFL.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(INTER_REGULAR_TTF_PATH "${GENERATED_DIR}/Inter-Regular.ttf")
set(INTER_SEMIBOLD_TTF_PATH "${GENERATED_DIR}/Inter-SemiBold.ttf")
set(INTER_OFL_TXT_PATH "${GENERATED_DIR}/Inter-OFL.txt")
set(INTER_REGULAR_HEADER_PATH "${GENERATED_DIR}/inter_regular_ttf.h")
set(INTER_SEMIBOLD_HEADER_PATH "${GENERATED_DIR}/inter_semibold_ttf.h")
set(INTER_OFL_HEADER_PATH "${GENERATED_DIR}/inter_ofl_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${INTER_REGULAR_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_REGULAR_URL} -DDEST=${INTER_REGULAR_TTF_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_REGULAR_TTF_PATH} -DOUTPUT=${INTER_REGULAR_HEADER_PATH}
            -DSYMBOL=kInterRegularTtf
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter Regular"
    VERBATIM
)

add_custom_command(
    OUTPUT ${INTER_SEMIBOLD_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_SEMIBOLD_URL} -DDEST=${INTER_SEMIBOLD_TTF_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_SEMIBOLD_TTF_PATH} -DOUTPUT=${INTER_SEMIBOLD_HEADER_PATH}
            -DSYMBOL=kInterSemiBoldTtf
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter SemiBold"
    VERBATIM
)

# The OFL license text itself gets embedded too, so the installer can write
# it into the *installed* tree at ~/.tuxblox/COPYRIGHT.txt
# (Task 7) -- end users get proper attribution without this repo needing to
# carry a separate third_party_licenses/ entry for a font that's only ever
# shipped inside the installer binary, never inside this repo's own tree.
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

add_custom_target(generate_font_header DEPENDS
    ${INTER_REGULAR_HEADER_PATH} ${INTER_SEMIBOLD_HEADER_PATH} ${INTER_OFL_HEADER_PATH})
