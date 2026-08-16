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

# Embeds license texts for third-party dependencies compiled directly into
# the launcher binary (nlohmann/json), so copyright_file.cpp can write
# proper attribution into COPYRIGHT.txt. Identical to
# installer/cmake/EmbedThirdPartyLicenses.cmake.

set(JSON_LICENSE_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(JSON_LICENSE_TXT_PATH "${GENERATED_DIR}/json-LICENSE.MIT")
set(JSON_LICENSE_HEADER_PATH "${GENERATED_DIR}/json_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${JSON_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${JSON_LICENSE_URL} -DDEST=${JSON_LICENSE_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${JSON_LICENSE_TXT_PATH} -DOUTPUT=${JSON_LICENSE_HEADER_PATH}
            -DSYMBOL=kJsonLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding nlohmann/json license text"
    VERBATIM
)

add_custom_target(generate_thirdparty_license_headers DEPENDS
    ${JSON_LICENSE_HEADER_PATH})
