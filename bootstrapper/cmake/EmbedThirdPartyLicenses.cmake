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
# the installer binary (Dear ImGui, nlohmann/json), so copyright_file.cpp
# can write proper attribution into the installed product's COPYRIGHT.txt.
# Mirrors FetchFont.cmake's fetch-or-locate + embed pattern.
#
# Dear ImGui already ships its own LICENSE.txt in the vendored tree
# (populated by vendor.sh before cmake runs), so it's embedded directly --
# no download needed. nlohmann/json is vendored as a single header with no
# accompanying LICENSE file, so its license is fetched separately, pinned
# to the exact same v3.11.3 tag vendor.sh already pins json.hpp to.
#
# The LGPLv3 text covers the Qt6 libraries the *launcher* bundles. It's
# embedded here too, not just in launcher/, because both binaries write the
# same ~/.tuxblox/COPYRIGHT.txt and so must be able to emit the same content
# -- see the comment at the top of copyright_file.cpp.
#
# stb_image.h's dual-license text lives inline in that (large, mostly-code)
# header rather than in a separate file, so it isn't handled here -- it's
# hardcoded directly in copyright_file.cpp instead, with a comment noting
# which pinned commit it was copied from. ICU 56's license and the
# libxcb/xkbcommon notices are hardcoded there for the same reason (ICU 56
# publishes its license only as HTML at that tag).

set(IMGUI_LICENSE_PATH "${CMAKE_SOURCE_DIR}/third_party/imgui/LICENSE.txt")
set(JSON_LICENSE_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT")
set(LGPL3_LICENSE_URL "https://www.gnu.org/licenses/lgpl-3.0.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(JSON_LICENSE_TXT_PATH "${GENERATED_DIR}/json-LICENSE.MIT")
set(LGPL3_LICENSE_TXT_PATH "${GENERATED_DIR}/lgpl-3.0.txt")
set(IMGUI_LICENSE_HEADER_PATH "${GENERATED_DIR}/imgui_license_txt.h")
set(JSON_LICENSE_HEADER_PATH "${GENERATED_DIR}/json_license_txt.h")
set(LGPL3_LICENSE_HEADER_PATH "${GENERATED_DIR}/lgpl3_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

if(NOT EXISTS ${IMGUI_LICENSE_PATH})
    message(FATAL_ERROR "Dear ImGui LICENSE.txt not found at ${IMGUI_LICENSE_PATH} -- run ./vendor.sh first.")
endif()

add_custom_command(
    OUTPUT ${IMGUI_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${IMGUI_LICENSE_PATH} -DOUTPUT=${IMGUI_LICENSE_HEADER_PATH}
            -DSYMBOL=kImguiLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${IMGUI_LICENSE_PATH}
    COMMENT "Embedding Dear ImGui license text"
    VERBATIM
)

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

add_custom_command(
    OUTPUT ${LGPL3_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${LGPL3_LICENSE_URL} -DDEST=${LGPL3_LICENSE_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${LGPL3_LICENSE_TXT_PATH} -DOUTPUT=${LGPL3_LICENSE_HEADER_PATH}
            -DSYMBOL=kLgpl3LicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding LGPLv3 license text (bundled Qt6)"
    VERBATIM
)

add_custom_target(generate_thirdparty_license_headers DEPENDS
    ${IMGUI_LICENSE_HEADER_PATH} ${JSON_LICENSE_HEADER_PATH} ${LGPL3_LICENSE_HEADER_PATH})
