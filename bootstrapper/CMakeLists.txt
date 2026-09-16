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
project(TuxBloxBootstrapper CXX)

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
# glibc >= 2.34 folded libpthread/libdl into libc, so std::thread links for
# free on a newer host -- the old-glibc container this builds in still needs
# them named.
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

add_library(bootstrapper_core STATIC
    src/cli.cpp
    src/config.cpp
    src/progress.cpp
    src/ui_scale.cpp
    src/install_plan.cpp
    src/roblox_deploy.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/checksum.cpp
    src/app.cpp
)
target_include_directories(bootstrapper_core PUBLIC
    src third_party
    ${CURL_INCLUDE_DIRS} ${ARCHIVE_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
# OpenSSL is linked statically for the same reason as the launcher and the
# installer: this builds against OpenSSL 1.1 in the container, and a host on
# OpenSSL 3 carries no libcrypto.so.1.1 for a dynamic link to find.
find_library(OPENSSL_SSL_STATIC_LIB NAMES libssl.a REQUIRED)
find_library(OPENSSL_CRYPTO_STATIC_LIB NAMES libcrypto.a REQUIRED)
target_link_libraries(bootstrapper_core PUBLIC
    ${CURL_LIBRARIES} ${ARCHIVE_LIBRARIES}
    ${OPENSSL_SSL_STATIC_LIB} ${OPENSSL_CRYPTO_STATIC_LIB} ${CMAKE_DL_LIBS}
    Threads::Threads)
# Keeps our static OpenSSL 1.1 symbols out of the dynamic symbol table, so
# libcurl cannot resolve its own OpenSSL 3 calls against them.
target_link_options(bootstrapper_core PUBLIC
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)
target_compile_options(bootstrapper_core PUBLIC
    ${CURL_CFLAGS_OTHER} ${ARCHIVE_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})

# Same version resolution as the launcher and the installer, so one release
# stamps the same number into all of them. The repo-root VERSION is only
# reachable for a bare host cmake run; inside the container only this
# directory is mounted, which is why build.sh forwards the value.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
if(NOT "$ENV{TUXBLOX_BUILD_VERSION}" STREQUAL "")
    set(TUXBLOX_BOOTSTRAPPER_VERSION "$ENV{TUXBLOX_BUILD_VERSION}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_BOOTSTRAPPER_VERSION LIMIT_COUNT 1)
else()
    set(TUXBLOX_BOOTSTRAPPER_VERSION "0.0.0-dev")
endif()
message(STATUS "TuxBlox bootstrapper version: ${TUXBLOX_BOOTSTRAPPER_VERSION}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
target_include_directories(bootstrapper_core PUBLIC ${CMAKE_BINARY_DIR}/generated)

include(cmake/FetchLogo.cmake)
include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)

add_executable(TuxBloxBootstrapper
    src/main.cpp
    src/ui.cpp
)
target_include_directories(TuxBloxBootstrapper PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxBootstrapper PRIVATE bootstrapper_core imgui ${SDL2_LIBRARIES} OpenGL::GL ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(TuxBloxBootstrapper generate_logo_header generate_font_header generate_thirdparty_license_headers)

enable_testing()
add_subdirectory(tests)
