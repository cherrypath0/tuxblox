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

# Downloads the launcher's two UI typefaces (both OFL-1.1) from jsDelivr's
# fontsource mirror, pinned to fixed releases, and generates C headers
# embedding the raw TTF bytes plus each family's license text.
#
# Inter is the body face and Montserrat the display face, matching
# tuxblox.net -- the site loads Inter 400/500/600 and Montserrat 600/700
# from Google Fonts, and these are the same weights.

set(INTER_VERSION "5.0.0")
set(MONTSERRAT_VERSION "5.0.0")
set(FONTSOURCE_BASE "https://cdn.jsdelivr.net/fontsource/fonts")
set(GOOGLE_FONTS_OFL "https://raw.githubusercontent.com/google/fonts/main/ofl")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")

file(MAKE_DIRECTORY ${GENERATED_DIR})

# Downloads one remote file and turns its bytes into a C header. The header
# name and the C symbol are separate arguments because they don't match --
# main.cpp includes "inter_regular_ttf.h" to get kInterRegularTtf[].
function(embedRemoteFile url rawName headerName symbol outVar)
    set(rawPath "${GENERATED_DIR}/${rawName}")
    set(headerPath "${GENERATED_DIR}/${headerName}")
    add_custom_command(
        OUTPUT ${headerPath}
        COMMAND ${CMAKE_COMMAND} -DURL=${url} -DDEST=${rawPath}
                -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
        COMMAND ${CMAKE_COMMAND} -DINPUT=${rawPath} -DOUTPUT=${headerPath}
                -DSYMBOL=${symbol}
                -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
        COMMENT "Fetching and embedding ${rawName}"
        VERBATIM
    )
    set(${outVar} ${headerPath} PARENT_SCOPE)
endfunction()

embedRemoteFile("${FONTSOURCE_BASE}/inter@${INTER_VERSION}/latin-400-normal.ttf"
    "Inter-Regular.ttf" "inter_regular_ttf.h" "kInterRegularTtf" INTER_REGULAR_HEADER)
embedRemoteFile("${FONTSOURCE_BASE}/inter@${INTER_VERSION}/latin-500-normal.ttf"
    "Inter-Medium.ttf" "inter_medium_ttf.h" "kInterMediumTtf" INTER_MEDIUM_HEADER)
embedRemoteFile("${FONTSOURCE_BASE}/inter@${INTER_VERSION}/latin-600-normal.ttf"
    "Inter-SemiBold.ttf" "inter_semibold_ttf.h" "kInterSemiBoldTtf" INTER_SEMIBOLD_HEADER)
embedRemoteFile("${GOOGLE_FONTS_OFL}/inter/OFL.txt"
    "Inter-OFL.txt" "inter_ofl_license_txt.h" "kInterOflLicenseTxt" INTER_OFL_HEADER)

embedRemoteFile("${FONTSOURCE_BASE}/montserrat@${MONTSERRAT_VERSION}/latin-600-normal.ttf"
    "Montserrat-SemiBold.ttf" "montserrat_semibold_ttf.h" "kMontserratSemiBoldTtf" MONTSERRAT_SEMIBOLD_HEADER)
embedRemoteFile("${FONTSOURCE_BASE}/montserrat@${MONTSERRAT_VERSION}/latin-700-normal.ttf"
    "Montserrat-Bold.ttf" "montserrat_bold_ttf.h" "kMontserratBoldTtf" MONTSERRAT_BOLD_HEADER)
embedRemoteFile("${GOOGLE_FONTS_OFL}/montserrat/OFL.txt"
    "Montserrat-OFL.txt" "montserrat_ofl_license_txt.h" "kMontserratOflLicenseTxt" MONTSERRAT_OFL_HEADER)

add_custom_target(generate_font_header DEPENDS
    ${INTER_REGULAR_HEADER} ${INTER_MEDIUM_HEADER} ${INTER_SEMIBOLD_HEADER} ${INTER_OFL_HEADER}
    ${MONTSERRAT_SEMIBOLD_HEADER} ${MONTSERRAT_BOLD_HEADER} ${MONTSERRAT_OFL_HEADER})
