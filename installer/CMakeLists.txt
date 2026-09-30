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

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "third_party/json.hpp is missing. Run ./vendor.sh first.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
pkg_check_modules(OPENSSL REQUIRED openssl)
# glibc >= 2.34 merged libpthread/libdl's symbols into libc.so itself, so std::thread
# links "for free" on a newer-glibc host -- but the old-glibc-baseline container this
# project builds inside (see build-container/Containerfile) still needs them explicit.
# Threads::Threads is load-bearing (std::thread is used); ${CMAKE_DL_LIBS} is
# defensive (nothing in this target calls dlopen/dlsym directly, but old-glibc-baseline
# safety makes it cheap insurance).
find_package(Threads REQUIRED)

add_library(installer_core STATIC
    src/cli.cpp
    src/manifest.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/checksum.cpp
    src/ui_payload.cpp
    src/ui_cache.cpp
    src/ui_extract.cpp
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

# Same version resolution as launcher/CMakeLists.txt: TUXBLOX_BUILD_VERSION (set
# by the root build.sh, or by installer/build.sh from the repo-root VERSION when
# run standalone, and forwarded into this build container either way) wins, so
# one release build stamps the launcher, the installer and the compatibility
# layer with the same number. The repo-root VERSION is the fallback for a bare
# host cmake run, where it is reachable; it is not, from inside the container.
# ${CMAKE_BINARY_DIR}/generated is already on TuxBloxInstaller's include path
# below, alongside the logo/font/license headers.
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
if(NOT "$ENV{TUXBLOX_BUILD_VERSION}" STREQUAL "")
    set(TUXBLOX_INSTALLER_VERSION "$ENV{TUXBLOX_BUILD_VERSION}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_INSTALLER_VERSION LIMIT_COUNT 1)
else()
    set(TUXBLOX_INSTALLER_VERSION "0.0.0-dev")
endif()
# The channel is resolved the same way and for the same reason: the installer
# defaults to installing its own version, which only exists on the channel it
# was built for.
if(NOT "$ENV{TUXBLOX_CHANNEL}" STREQUAL "")
    set(TUXBLOX_INSTALLER_CHANNEL "$ENV{TUXBLOX_CHANNEL}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_VERSION_LINES LIMIT_COUNT 2)
    list(LENGTH TUXBLOX_VERSION_LINES TUXBLOX_VERSION_LINE_COUNT)
    if(TUXBLOX_VERSION_LINE_COUNT GREATER 1)
        list(GET TUXBLOX_VERSION_LINES 1 TUXBLOX_INSTALLER_CHANNEL)
    else()
        set(TUXBLOX_INSTALLER_CHANNEL "stable")
    endif()
else()
    set(TUXBLOX_INSTALLER_CHANNEL "stable")
endif()
message(STATUS "TuxBlox installer version: ${TUXBLOX_INSTALLER_VERSION}")
message(STATUS "TuxBlox installer channel: ${TUXBLOX_INSTALLER_CHANNEL}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
# installer_core compiles cli.cpp, which needs the generated version/channel.
# PUBLIC so TuxBloxInstaller inherits it, and placed after configure_file so the
# header exists by the time anything is told to look for it.
target_include_directories(installer_core PUBLIC ${CMAKE_BINARY_DIR}/generated)

include(cmake/FetchLogo.cmake)
include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)

add_executable(TuxBloxInstaller
    src/main.cpp
    src/app.cpp
    src/console_ui.cpp
    src/desktop_shortcut.cpp
    src/copyright_file.cpp
)
target_include_directories(TuxBloxInstaller PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxInstaller PRIVATE installer_core ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(TuxBloxInstaller generate_logo_header generate_font_header generate_thirdparty_license_headers)

# The interface binary, which lives inside the appended payload rather than
# beside the installer, so its libraries are found relative to itself.
if(TUXBLOX_UI_STACK_DEV)
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

    add_executable(TuxBloxInstaller-ui
        src/ui_main.cpp
        src/app.cpp
        src/ui_adw.cpp
        src/console_ui.cpp
        src/desktop_shortcut.cpp
        src/copyright_file.cpp
    )
    target_include_directories(TuxBloxInstaller-ui PRIVATE
        ${CMAKE_BINARY_DIR}/generated ${ADWAITA_INCLUDE_DIRS})
    target_compile_options(TuxBloxInstaller-ui PRIVATE ${ADWAITA_CFLAGS_OTHER})
    target_link_libraries(TuxBloxInstaller-ui PRIVATE
        installer_core ${ADWAITA_LINK_LIBRARIES} ${CMAKE_DL_LIBS} Threads::Threads)
    set_target_properties(TuxBloxInstaller-ui PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/lib/x86_64-linux-gnu:$ORIGIN/lib")
    add_dependencies(TuxBloxInstaller-ui generate_logo_header generate_thirdparty_license_headers)
endif()

enable_testing()
add_subdirectory(tests)
