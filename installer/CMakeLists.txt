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

# The interface sources shared with the other programs live outside this directory. The build container mounts them and passes the path; a bare host cmake run finds them beside it.
if(NOT TUXBLOX_UI_SRC)
    set(TUXBLOX_UI_SRC "${CMAKE_SOURCE_DIR}/../shared/ui/src")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "third_party/json.hpp is missing. Run ./vendor.sh first.")
endif()

# Still needed by the interface stack's UiStack.cmake, which finds GTK and libadwaita through pkg-config.
find_package(PkgConfig REQUIRED)

# TuxBlox's own code that uses those libraries, mounted read-only by build.sh.
if(NOT TUXBLOX_SHARED_CRYPTO)
    set(TUXBLOX_SHARED_CRYPTO "${CMAKE_SOURCE_DIR}/../shared/crypto/src")
endif()

# The libraries TuxBlox ships rather than taking from the machine, built by shared/tools/libs-build.sh and mounted read-only by build.sh.
if(NOT TUXBLOX_SHARED_DIR)
    set(TUXBLOX_SHARED_DIR "${CMAKE_SOURCE_DIR}/../build/.artifacts/shared")
endif()
if(NOT EXISTS "${TUXBLOX_SHARED_DIR}/lib/libcurl.a")
    message(FATAL_ERROR "${TUXBLOX_SHARED_DIR}/lib/libcurl.a is missing. Run ../shared/tools/libs-build.sh first.")
endif()
set(TUXBLOX_SHARED_INCLUDE "${TUXBLOX_SHARED_DIR}/include")
# Static archives are order sensitive: each must come before whatever it calls into.
set(TUXBLOX_SHARED_LIBS
    "${TUXBLOX_SHARED_DIR}/lib/libcurl.a"
    "${TUXBLOX_SHARED_DIR}/lib/libnghttp2.a"
    "${TUXBLOX_SHARED_DIR}/lib/libarchive.a"
    "${TUXBLOX_SHARED_DIR}/lib/libzstd.a"
    "${TUXBLOX_SHARED_DIR}/lib/libz.a"
    "${TUXBLOX_SHARED_DIR}/lib/libssl.a"
    "${TUXBLOX_SHARED_DIR}/lib/libcrypto.a")
# glibc >= 2.34 merged libpthread/libdl's symbols into libc.so itself, so std::thread
# links "for free" on a newer-glibc host -- but the old-glibc-baseline container this
# project builds inside (see build-container/Containerfile) still needs them explicit.
# Threads::Threads is load-bearing (std::thread is used); ${CMAKE_DL_LIBS} is
# defensive (nothing in this target calls dlopen/dlsym directly, but old-glibc-baseline
# safety makes it cheap insurance).
find_package(Threads REQUIRED)

add_library(installer_core STATIC
    ${TUXBLOX_SHARED_CRYPTO}/ca_bundle.cpp
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
include(${TUXBLOX_SHARED_CRYPTO}/EmbedCaBundle.cmake)

target_include_directories(installer_core PUBLIC
    src third_party ${TUXBLOX_SHARED_INCLUDE} ${TUXBLOX_SHARED_CRYPTO} ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(installer_core PUBLIC
    ${TUXBLOX_SHARED_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
# --exclude-libs keeps these archives' symbols out of the binary's dynamic symbol table. They are
# still linked in and callable from our own code; this only controls whether they are exported.
# It is load-bearing for zlib, which the interface binary holds twice on purpose: a static copy
# inside libarchive.a here, and the stack's own libz.so.1, which its libtiff and libfreetype both
# load. Exported, our static inflate and crc32 would be handed to them instead of the stack's.
target_link_options(installer_core PUBLIC
    -Wl,--exclude-libs=libcurl.a -Wl,--exclude-libs=libnghttp2.a
    -Wl,--exclude-libs=libarchive.a -Wl,--exclude-libs=libzstd.a
    -Wl,--exclude-libs=libz.a
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)
add_dependencies(installer_core generate_ca_bundle_header)

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
if(TUXBLOX_LIBADWAITA_COMMIT)
    target_compile_definitions(TuxBloxInstaller PRIVATE TUXBLOX_LIBADWAITA_COMMIT="${TUXBLOX_LIBADWAITA_COMMIT}")
endif()

# The interface binary, which lives inside the appended payload rather than
# beside the installer, so its libraries are found relative to itself.
if(TUXBLOX_UI_STACK_DEV)
    include(${TUXBLOX_UI_SRC}/UiStack.cmake)

    add_executable(TuxBloxInstaller-ui
        src/ui_main.cpp
        src/app.cpp
        src/ui_adw.cpp
        src/console_ui.cpp
        src/desktop_shortcut.cpp
        src/copyright_file.cpp
        ${TUXBLOX_UI_SRC}/adw_env.cpp
        ${TUXBLOX_UI_SRC}/adw_look.cpp
    )
    target_include_directories(TuxBloxInstaller-ui PRIVATE
        ${CMAKE_BINARY_DIR}/generated ${TUXBLOX_UI_SRC} ${ADWAITA_INCLUDE_DIRS})
    target_compile_options(TuxBloxInstaller-ui PRIVATE ${ADWAITA_CFLAGS_OTHER})
    target_link_libraries(TuxBloxInstaller-ui PRIVATE
        installer_core ${ADWAITA_LINK_LIBRARIES} ${CMAKE_DL_LIBS} Threads::Threads)
    set_target_properties(TuxBloxInstaller-ui PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/lib/x86_64-linux-gnu:$ORIGIN/lib")
    add_dependencies(TuxBloxInstaller-ui generate_logo_header generate_font_header generate_thirdparty_license_headers)
    if(TUXBLOX_LIBADWAITA_COMMIT)
        target_compile_definitions(TuxBloxInstaller-ui PRIVATE TUXBLOX_LIBADWAITA_COMMIT="${TUXBLOX_LIBADWAITA_COMMIT}")
    endif()
endif()

enable_testing()
add_subdirectory(tests)
