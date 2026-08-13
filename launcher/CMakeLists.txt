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
project(TuxBloxLauncher CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.h")
    message(FATAL_ERROR "third_party/imgui is missing. Run ./vendor.sh first.")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "third_party/json.hpp is missing. Run ./vendor.sh first.")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/stb_image.h")
    message(FATAL_ERROR "third_party/stb_image.h is missing. Run ./vendor.sh first.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
pkg_check_modules(OPENSSL REQUIRED openssl)
find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends ${SDL2_INCLUDE_DIRS})
target_compile_options(imgui PRIVATE ${SDL2_CFLAGS_OTHER})

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" TUXBLOX_LAUNCHER_VERSION LIMIT_COUNT 1)
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

add_library(launcher_core STATIC
    src/install_paths.cpp
    src/checksum.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/manifest.cpp
    src/lnk_resolver.cpp
    src/process_launcher.cpp
    src/updater.cpp
    src/headless_launch.cpp
    src/desktop_integration.cpp
    src/copyright_file.cpp
    src/settings.cpp
    src/crash_report.cpp
    src/container_env.cpp
)
target_include_directories(launcher_core PUBLIC
    src third_party ${CMAKE_BINARY_DIR}/generated
    ${CURL_INCLUDE_DIRS} ${ARCHIVE_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(launcher_core PUBLIC
    ${CURL_LIBRARIES} ${ARCHIVE_LIBRARIES} ${OPENSSL_LIBRARIES})
target_compile_options(launcher_core PUBLIC
    ${CURL_CFLAGS_OTHER} ${ARCHIVE_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})

include(cmake/FetchLogo.cmake)
include(cmake/FetchWindowIcon.cmake)
include(cmake/FetchFont.cmake)
include(cmake/EmbedIcons.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)

add_executable(TuxBloxLauncher
    src/main.cpp
    src/app.cpp
    src/ui.cpp
    src/watch_launch.cpp
)
target_include_directories(TuxBloxLauncher PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxLauncher PRIVATE launcher_core imgui ${SDL2_LIBRARIES} OpenGL::GL ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(TuxBloxLauncher generate_logo_header generate_window_icon_header generate_font_header generate_icon_headers generate_thirdparty_license_headers)

enable_testing()
add_subdirectory(tests)
