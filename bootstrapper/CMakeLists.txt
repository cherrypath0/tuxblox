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
# glibc >= 2.34 folded libpthread/libdl into libc, so std::thread links for
# free on a newer host -- the old-glibc container this builds in still needs
# them named.
find_package(Threads REQUIRED)

add_library(bootstrapper_core STATIC
    ${TUXBLOX_SHARED_CRYPTO}/ca_bundle.cpp
    src/cli.cpp
    src/config.cpp
    src/progress.cpp
    src/install_plan.cpp
    src/roblox_deploy.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/checksum.cpp
    src/app.cpp
)
include(${TUXBLOX_SHARED_CRYPTO}/EmbedCaBundle.cmake)

target_include_directories(bootstrapper_core PUBLIC
    src third_party ${TUXBLOX_SHARED_INCLUDE} ${TUXBLOX_SHARED_CRYPTO} ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(bootstrapper_core PUBLIC
    ${TUXBLOX_SHARED_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
# --exclude-libs keeps these archives' symbols out of the binary's dynamic symbol table. They are
# still linked in and callable from our own code; this only controls whether they are exported.
# It is load-bearing for zlib, which this process holds twice on purpose: a static copy inside
# libarchive.a here, and the interface stack's own libz.so.1, which its libtiff and libfreetype
# both load. Exported, our static inflate and crc32 would be handed to them instead of the stack's.
target_link_options(bootstrapper_core PUBLIC
    -Wl,--exclude-libs=libcurl.a -Wl,--exclude-libs=libnghttp2.a
    -Wl,--exclude-libs=libarchive.a -Wl,--exclude-libs=libzstd.a
    -Wl,--exclude-libs=libz.a
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)
add_dependencies(bootstrapper_core generate_ca_bundle_header)

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
# The channel is resolved the same way, from line 2 of the same file, and is
# baked in so --version can report "x.y.z-channel" for the launcher's
# mixed-install check.
if(NOT "$ENV{TUXBLOX_CHANNEL}" STREQUAL "")
    set(TUXBLOX_BOOTSTRAPPER_CHANNEL "$ENV{TUXBLOX_CHANNEL}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_VERSION_LINES LIMIT_COUNT 2)
    list(LENGTH TUXBLOX_VERSION_LINES TUXBLOX_VERSION_LINE_COUNT)
    if(TUXBLOX_VERSION_LINE_COUNT GREATER 1)
        list(GET TUXBLOX_VERSION_LINES 1 TUXBLOX_BOOTSTRAPPER_CHANNEL)
    else()
        set(TUXBLOX_BOOTSTRAPPER_CHANNEL "stable")
    endif()
else()
    set(TUXBLOX_BOOTSTRAPPER_CHANNEL "stable")
endif()
message(STATUS "TuxBlox bootstrapper version: ${TUXBLOX_BOOTSTRAPPER_VERSION}")
message(STATUS "TuxBlox bootstrapper channel: ${TUXBLOX_BOOTSTRAPPER_CHANNEL}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)
target_include_directories(bootstrapper_core PUBLIC ${CMAKE_BINARY_DIR}/generated)

include(cmake/FetchLogo.cmake)

# The interface binary needs the stack's development tree, which only exists once shared/tools/ui-build.sh has run; without it only the tests build, and build.sh refuses to finish.
if(TUXBLOX_UI_STACK_DEV)
    include(${TUXBLOX_UI_SRC}/UiStack.cmake)

    add_executable(TuxBloxBootstrapper
        src/main.cpp
        src/ui_adw.cpp
        ${TUXBLOX_UI_SRC}/adw_env.cpp
        ${TUXBLOX_UI_SRC}/adw_look.cpp
    )
    target_include_directories(TuxBloxBootstrapper PRIVATE
        ${CMAKE_BINARY_DIR}/generated ${TUXBLOX_UI_SRC} ${ADWAITA_INCLUDE_DIRS})
    target_compile_options(TuxBloxBootstrapper PRIVATE ${ADWAITA_CFLAGS_OTHER})
    target_link_libraries(TuxBloxBootstrapper PRIVATE
        bootstrapper_core ${ADWAITA_LINK_LIBRARIES} ${CMAKE_DL_LIBS} Threads::Threads)
    # The libraries are installed in the libtuxblox folder beside this binary.
    set_target_properties(TuxBloxBootstrapper PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/libtuxblox/lib/x86_64-linux-gnu:$ORIGIN/libtuxblox/lib")
    add_dependencies(TuxBloxBootstrapper generate_logo_header)
endif()

enable_testing()
add_subdirectory(tests)
