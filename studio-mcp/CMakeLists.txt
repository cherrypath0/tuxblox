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

cmake_minimum_required(VERSION 3.16)
project(TuxBloxStudioMcp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Deliberately links nothing but libc and libstdc++: an MCP client starts this
# on a machine that may have no TuxBlox GUI dependencies installed at all, so
# every source below is one of the launcher's Qt-free, curl-free units.
#
# The launcher tree is a sibling in the repository, but the build container
# mounts only this directory at /src, so build.sh mounts launcher/ separately
# and points this at it.
set(TUXBLOX_LAUNCHER_DIR "${CMAKE_SOURCE_DIR}/../launcher" CACHE PATH
    "Path to the launcher source tree this shares its path-resolution code with")
if(NOT EXISTS "${TUXBLOX_LAUNCHER_DIR}/src/launch_paths.cpp")
    message(FATAL_ERROR
        "TUXBLOX_LAUNCHER_DIR (${TUXBLOX_LAUNCHER_DIR}) is not a launcher source tree.")
endif()
# Not in the repository -- launcher/vendor.sh fetches it, which build.sh runs.
if(NOT EXISTS "${TUXBLOX_LAUNCHER_DIR}/third_party/json.hpp")
    message(FATAL_ERROR
        "${TUXBLOX_LAUNCHER_DIR}/third_party/json.hpp is missing. "
        "Run launcher/vendor.sh first.")
endif()

# TUXBLOX_BUILD_VERSION is the version the root build.sh resolved, forwarded
# into this container because the repo-root VERSION is not reachable from
# inside it. The file is the fallback for a bare host cmake run.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
if(NOT "$ENV{TUXBLOX_BUILD_VERSION}" STREQUAL "")
    set(TUXBLOX_MCP_VERSION "$ENV{TUXBLOX_BUILD_VERSION}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_MCP_VERSION LIMIT_COUNT 1)
else()
    set(TUXBLOX_MCP_VERSION "0.0.0-dev")
endif()
# The channel is resolved the same way, from line 2 of the same file.
if(NOT "$ENV{TUXBLOX_CHANNEL}" STREQUAL "")
    set(TUXBLOX_MCP_CHANNEL "$ENV{TUXBLOX_CHANNEL}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_VERSION_LINES LIMIT_COUNT 2)
    list(LENGTH TUXBLOX_VERSION_LINES TUXBLOX_VERSION_LINE_COUNT)
    if(TUXBLOX_VERSION_LINE_COUNT GREATER 1)
        list(GET TUXBLOX_VERSION_LINES 1 TUXBLOX_MCP_CHANNEL)
    else()
        set(TUXBLOX_MCP_CHANNEL "stable")
    endif()
else()
    set(TUXBLOX_MCP_CHANNEL "stable")
endif()
message(STATUS "TuxBlox Studio MCP version: ${TUXBLOX_MCP_VERSION}")
message(STATUS "TuxBlox Studio MCP channel: ${TUXBLOX_MCP_CHANNEL}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in
               ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

add_executable(studio-mcp
    src/main.cpp
    src/mcp_locate.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/install_paths.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/launch_paths.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/lnk_resolver.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/settings.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/system_info.cpp
    ${TUXBLOX_LAUNCHER_DIR}/src/versions_manifest.cpp
)
target_include_directories(studio-mcp PRIVATE
    src
    ${TUXBLOX_LAUNCHER_DIR}/src
    ${TUXBLOX_LAUNCHER_DIR}/third_party
    ${CMAKE_BINARY_DIR}/generated
)

enable_testing()
add_subdirectory(tests)
