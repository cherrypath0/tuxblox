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

# Same reason as the launcher's suite: Release defines -DNDEBUG, which would
# compile every assert() below to nothing and turn the suite into "always
# passes". Scoped to this directory so the shipped binary is unaffected.
add_compile_options(-UNDEBUG)

add_executable(test_mcp_locate
    test_mcp_locate.cpp
    ${CMAKE_SOURCE_DIR}/src/mcp_locate.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/install_paths.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/launch_paths.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/lnk_resolver.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/versions_manifest.cpp
)
target_include_directories(test_mcp_locate PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${TUXBLOX_LAUNCHER_DIR}/src
    ${TUXBLOX_LAUNCHER_DIR}/third_party
)
add_test(NAME mcp_locate COMMAND test_mcp_locate)
