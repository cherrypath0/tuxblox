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

# Builds the flag defaults into the binary. They are a build input, never shipped: what a
# release does with no server and no override is fixed at the moment it is compiled.

if(NOT TUXBLOX_FLAGS_DIR)
    set(TUXBLOX_FLAGS_DIR "${CMAKE_CURRENT_LIST_DIR}/..")
endif()

# Only x86_64 is built today; arm64 is reserved in the release layout and gets its file
# when it gets a build.
if(NOT TUXBLOX_FLAGS_ARCH)
    set(TUXBLOX_FLAGS_ARCH "x86_64")
endif()

set(DEFAULT_FLAGS_JSON "${TUXBLOX_FLAGS_DIR}/DefaultFlags-${TUXBLOX_FLAGS_ARCH}.json")
set(DEFAULT_FLAGS_HEADER "${CMAKE_BINARY_DIR}/generated/tuxblox_default_flags.h")

find_program(PYTHON3_EXECUTABLE python3)
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 is required to build in the flag defaults.")
endif()

if(NOT EXISTS "${DEFAULT_FLAGS_JSON}")
    message(FATAL_ERROR "${DEFAULT_FLAGS_JSON} is missing -- there is no answer for a build with no server.")
endif()

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

add_custom_command(
    OUTPUT ${DEFAULT_FLAGS_HEADER}
    COMMAND ${PYTHON3_EXECUTABLE} "${TUXBLOX_FLAGS_DIR}/tools/embed-flags.py"
            "${DEFAULT_FLAGS_JSON}" "${DEFAULT_FLAGS_HEADER}" kTuxBloxDefaultFlagsJson
    DEPENDS "${DEFAULT_FLAGS_JSON}" "${TUXBLOX_FLAGS_DIR}/tools/embed-flags.py"
    COMMENT "Building in the flag defaults"
    VERBATIM
)

add_custom_target(generate_default_flags_header DEPENDS ${DEFAULT_FLAGS_HEADER})
