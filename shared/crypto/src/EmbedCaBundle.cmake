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

# Builds in a list of certificates, used only on a machine that has none of its own.

# Kept in the repository rather than downloaded, so a build needs no network.
# Refresh from https://curl.se/ca/cacert.pem.
get_filename_component(CA_BUNDLE_SRC "${CMAKE_CURRENT_LIST_DIR}/../data/cacert.pem" ABSOLUTE)
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(CA_BUNDLE_PEM_PATH "${GENERATED_DIR}/tuxblox-ca-bundle.pem")
set(CA_BUNDLE_HEADER_PATH "${GENERATED_DIR}/tuxblox_ca_bundle_pem.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${CA_BUNDLE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -E copy ${CA_BUNDLE_SRC} ${CA_BUNDLE_PEM_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${CA_BUNDLE_PEM_PATH} -DOUTPUT=${CA_BUNDLE_HEADER_PATH}
            -DSYMBOL=kTuxBloxCaBundlePem
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${CA_BUNDLE_SRC}
    COMMENT "Building in the certificate list"
    VERBATIM
)

add_custom_target(generate_ca_bundle_header DEPENDS ${CA_BUNDLE_HEADER_PATH})
