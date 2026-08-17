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
project(TuxBloxInstaller CXX)

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
# glibc >= 2.34 merged libpthread/libdl's symbols into libc.so itself, so std::thread
# links "for free" on a newer-glibc host -- but the old-glibc-baseline container this
# project builds inside (see build-container/Containerfile) still needs them explicit.
# Threads::Threads is load-bearing (std::thread is used); ${CMAKE_DL_LIBS} is
# defensive (nothing in this target calls dlopen/dlsym directly, but SDL2/OpenGL's
# transitive needs and general old-glibc-baseline safety make it cheap insurance).
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

add_library(installer_core STATIC
    src/cli.cpp
    src/manifest.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/checksum.cpp
    src/install_paths.cpp
    src/progress.cpp
    src/installer_steps.cpp
    src/container_env.cpp
    src/uninstall.cpp
)
target_include_directories(installer_core PUBLIC
    src third_party
    ${CURL_INCLUDE_DIRS} ${ARCHIVE_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
# Statically link OpenSSL rather than ${OPENSSL_LIBRARIES} (dynamic) -- this
# binary is built inside the pinned Ubuntu 20.04 container, which ships
# OpenSSL 1.1 (libcrypto.so.1.1). Distros that have since moved to OpenSSL 3
# (e.g. Arch) don't carry a libcrypto.so.1.1 compat package by default, so a
# dynamically-linked binary built here fails to even start elsewhere with
# "error while loading shared libraries: libcrypto.so.1.1: cannot open
# shared object file". Static linking bakes the exact OpenSSL 1.1 code this
# binary was built and tested against directly into it, removing the host
# OpenSSL version as a runtime dependency entirely. CURL_LIBRARIES/
# ARCHIVE_LIBRARIES stay dynamic -- their SONAMEs (libcurl.so.4,
# libarchive.so.13) are far more stable across distros and don't hit this.
find_library(OPENSSL_SSL_STATIC_LIB NAMES libssl.a REQUIRED)
find_library(OPENSSL_CRYPTO_STATIC_LIB NAMES libcrypto.a REQUIRED)
target_link_libraries(installer_core PUBLIC
    ${CURL_LIBRARIES} ${ARCHIVE_LIBRARIES}
    ${OPENSSL_SSL_STATIC_LIB} ${OPENSSL_CRYPTO_STATIC_LIB} ${CMAKE_DL_LIBS}
    Threads::Threads)
# --exclude-libs keeps libssl.a/libcrypto.a's symbols out of the final
# binary's DYNAMIC symbol table (still linked in and callable from our own
# code -- this only controls export visibility, not linkage). Without this,
# our statically-linked OpenSSL 1.1 exports symbols like
# EC_GROUP_order_bits/ENGINE_init globally, which the dynamic linker can
# then hand to libcurl.so.4's own dynamically-loaded libssl.so.3/
# libcrypto.so.3 (OpenSSL 3, on any host where that's what's installed)
# instead of its own -- two different, ABI-incompatible OpenSSL major
# versions calling into each other's functions with each other's struct
# layouts. Confirmed (identical symptom, same fix, in launcher/CMakeLists.txt)
# via gdb backtrace: SIGSEGV in EC_GROUP_order_bits reached via libcurl's
# own TLS setup, on the first network call after switching OpenSSL to
# static linking.
target_link_options(installer_core PUBLIC
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)
target_compile_options(installer_core PUBLIC
    ${CURL_CFLAGS_OTHER} ${ARCHIVE_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})

include(cmake/FetchLogo.cmake)
include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)

add_executable(TuxBloxInstaller
    src/main.cpp
    src/app.cpp
    src/ui.cpp
    src/console_ui.cpp
    src/desktop_shortcut.cpp
    src/copyright_file.cpp
)
target_include_directories(TuxBloxInstaller PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxInstaller PRIVATE installer_core imgui ${SDL2_LIBRARIES} OpenGL::GL ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(TuxBloxInstaller generate_logo_header generate_font_header generate_thirdparty_license_headers)

enable_testing()
add_subdirectory(tests)
