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

# Embeds TuxBlox's own repo-root LICENSE (GPLv3) into the launcher binary,
# so license_file.cpp can write it out to ~/.tuxblox/LICENSE on first run.

set(TUXBLOX_LICENSE_PATH "${CMAKE_SOURCE_DIR}/../LICENSE")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(TUXBLOX_LICENSE_HEADER_PATH "${GENERATED_DIR}/tuxblox_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

if(NOT EXISTS ${TUXBLOX_LICENSE_PATH})
    message(FATAL_ERROR "TuxBlox LICENSE not found at ${TUXBLOX_LICENSE_PATH}")
endif()

add_custom_command(
    OUTPUT ${TUXBLOX_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${TUXBLOX_LICENSE_PATH} -DOUTPUT=${TUXBLOX_LICENSE_HEADER_PATH}
            -DSYMBOL=kTuxBloxLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${TUXBLOX_LICENSE_PATH}
    COMMENT "Embedding TuxBlox LICENSE text"
    VERBATIM
)

add_custom_target(generate_license_header DEPENDS ${TUXBLOX_LICENSE_HEADER_PATH})
