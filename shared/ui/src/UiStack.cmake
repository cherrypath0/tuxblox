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

# Expects TUXBLOX_UI_STACK_DEV, sets ADWAITA_INCLUDE_DIRS, ADWAITA_CFLAGS_OTHER and ADWAITA_LINK_LIBRARIES. Included rather than called so the variables land in the caller's scope.
# Private requirements only matter to a static link, and would demand every -dev package the stack was built against from this container
set(UI_PC_DIR "${CMAKE_BINARY_DIR}/ui-pkgconfig")
file(REMOVE_RECURSE "${UI_PC_DIR}")
file(MAKE_DIRECTORY "${UI_PC_DIR}")
file(GLOB UI_PC_FILES "${TUXBLOX_UI_STACK_DEV}/lib/pkgconfig/*.pc" "${TUXBLOX_UI_STACK_DEV}/lib/x86_64-linux-gnu/pkgconfig/*.pc")
foreach(UI_PC_FILE ${UI_PC_FILES})
    file(STRINGS "${UI_PC_FILE}" UI_PC_LINES)
    list(FILTER UI_PC_LINES EXCLUDE REGEX "^Requires\\.private:")
    get_filename_component(UI_PC_NAME "${UI_PC_FILE}" NAME)
    get_filename_component(UI_PC_ORIGIN "${UI_PC_FILE}" DIRECTORY)
    string(REPLACE ";" "\n" UI_PC_TEXT "${UI_PC_LINES}")
    string(REPLACE "\${pcfiledir}" "${UI_PC_ORIGIN}" UI_PC_TEXT "${UI_PC_TEXT}")
    file(WRITE "${UI_PC_DIR}/${UI_PC_NAME}" "${UI_PC_TEXT}\n")
endforeach()
set(ENV{PKG_CONFIG_PATH} "${UI_PC_DIR}")
pkg_check_modules(ADWAITA REQUIRED libadwaita-1)
