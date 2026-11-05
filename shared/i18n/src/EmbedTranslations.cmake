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

# Turns the JSON catalogs into the C++ table the binaries embed, so no locale
# files are needed at runtime. Expects TUXBLOX_I18N_DIR; sets I18N_HEADER and
# creates the generate_i18n_header target.

find_program(PYTHON3_EXECUTABLE python3)
if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "python3 is required to embed the translation catalogs.")
endif()

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})
set(I18N_HEADER "${GENERATED_DIR}/i18n_data.h")

# Picked up here rather than listed in each project, so adding a catalog does
# not need a matching CMake edit.
file(GLOB I18N_CATALOGS CONFIGURE_DEPENDS "${TUXBLOX_I18N_DIR}/translations/*.json")

add_custom_command(
    OUTPUT "${I18N_HEADER}"
    COMMAND ${PYTHON3_EXECUTABLE} "${TUXBLOX_I18N_DIR}/tools/embed-translations.py"
            --input-dir "${TUXBLOX_I18N_DIR}/translations" --output "${I18N_HEADER}"
    DEPENDS ${I18N_CATALOGS} "${TUXBLOX_I18N_DIR}/tools/embed-translations.py"
    COMMENT "Embedding translations"
    VERBATIM
)

add_custom_target(generate_i18n_header DEPENDS "${I18N_HEADER}")
