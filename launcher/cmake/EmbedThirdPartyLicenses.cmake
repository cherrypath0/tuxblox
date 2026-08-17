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

# Embeds the license texts copyright_file.cpp needs to write proper
# attribution into the installed product's COPYRIGHT.txt. Mirrors
# installer/cmake/EmbedThirdPartyLicenses.cmake.
#
# The set of texts is deliberately the *product's*, not this binary's: the
# launcher and the installer both write the same ~/.tuxblox/COPYRIGHT.txt and
# whichever ran last wins, so both must be able to emit identical content --
# see the comment at the top of copyright_file.cpp. That's why the Dear ImGui
# license is embedded here even though only the installer uses ImGui, and why
# the LGPLv3 text is embedded in the installer even though only the launcher
# bundles Qt6.
#
# Unlike the installer, this tree doesn't vendor ImGui, so there's no local
# LICENSE.txt to read -- it's fetched instead, pinned to the same v1.91.0 tag
# installer/vendor.sh pins the ImGui source to.
#
# stb_image.h's dual-license text, ICU 56's license and the libxcb/xkbcommon
# notices are hardcoded in copyright_file.cpp rather than fetched here: stb
# keeps its text inline in the (large, mostly-code) header, and ICU 56
# publishes its license only as HTML at the release-56-1 tag.

set(IMGUI_LICENSE_URL "https://raw.githubusercontent.com/ocornut/imgui/v1.91.0/LICENSE.txt")
set(JSON_LICENSE_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT")
set(LGPL3_LICENSE_URL "https://www.gnu.org/licenses/lgpl-3.0.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(IMGUI_LICENSE_TXT_PATH "${GENERATED_DIR}/imgui-LICENSE.txt")
set(JSON_LICENSE_TXT_PATH "${GENERATED_DIR}/json-LICENSE.MIT")
set(LGPL3_LICENSE_TXT_PATH "${GENERATED_DIR}/lgpl-3.0.txt")
set(IMGUI_LICENSE_HEADER_PATH "${GENERATED_DIR}/imgui_license_txt.h")
set(JSON_LICENSE_HEADER_PATH "${GENERATED_DIR}/json_license_txt.h")
set(LGPL3_LICENSE_HEADER_PATH "${GENERATED_DIR}/lgpl3_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${IMGUI_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${IMGUI_LICENSE_URL} -DDEST=${IMGUI_LICENSE_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${IMGUI_LICENSE_TXT_PATH} -DOUTPUT=${IMGUI_LICENSE_HEADER_PATH}
            -DSYMBOL=kImguiLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Dear ImGui license text"
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
