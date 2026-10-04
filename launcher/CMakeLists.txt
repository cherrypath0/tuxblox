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

# The interface sources shared with the other programs live outside this directory. The build container mounts them and passes the path; a bare host cmake run finds them beside it.
if(NOT TUXBLOX_UI_SRC)
    set(TUXBLOX_UI_SRC "${CMAKE_SOURCE_DIR}/../shared/ui/src")
endif()

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "third_party/json.hpp is missing. Run ./vendor.sh first.")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/stb_image.h")
    message(FATAL_ERROR "third_party/stb_image.h is missing. Run ./vendor.sh first.")
endif()

# Still needed by the interface stack's UiStack.cmake, which finds GTK and libadwaita through pkg-config.
find_package(PkgConfig REQUIRED)

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
# defensive (nothing in this target calls dlopen/dlsym directly, but general
# old-glibc-baseline safety makes it cheap insurance).
find_package(Threads REQUIRED)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
# TUXBLOX_BUILD_VERSION is the version the root build.sh resolved; it wins so
# that one release build stamps the same number into the launcher, the installer
# and the compatibility layer. launcher/build.sh sets it from the repo-root
# VERSION file when run standalone, so in practice it is always set.
#
# The repo-root VERSION is only the second choice because it is not reachable
# from inside the build container -- only launcher/ is mounted there. It still
# resolves for a bare `cmake -B build -S .` run on the host, which is the one
# case that reaches it. Anything else gets a version that is obviously a
# placeholder rather than a plausible-looking wrong number.
if(NOT "$ENV{TUXBLOX_BUILD_VERSION}" STREQUAL "")
    set(TUXBLOX_LAUNCHER_VERSION "$ENV{TUXBLOX_BUILD_VERSION}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_LAUNCHER_VERSION LIMIT_COUNT 1)
else()
    set(TUXBLOX_LAUNCHER_VERSION "0.0.0-dev")
endif()
# The channel is resolved the same way, from line 2 of the same file. It is
# baked in because "which channel is this build from" has to survive the user
# switching channels in Settings, which changes what they want, not what runs.
if(NOT "$ENV{TUXBLOX_CHANNEL}" STREQUAL "")
    set(TUXBLOX_LAUNCHER_CHANNEL "$ENV{TUXBLOX_CHANNEL}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/../VERSION")
    file(STRINGS "${CMAKE_SOURCE_DIR}/../VERSION" TUXBLOX_VERSION_LINES LIMIT_COUNT 2)
    list(LENGTH TUXBLOX_VERSION_LINES TUXBLOX_VERSION_LINE_COUNT)
    if(TUXBLOX_VERSION_LINE_COUNT GREATER 1)
        list(GET TUXBLOX_VERSION_LINES 1 TUXBLOX_LAUNCHER_CHANNEL)
    else()
        set(TUXBLOX_LAUNCHER_CHANNEL "stable")
    endif()
else()
    set(TUXBLOX_LAUNCHER_CHANNEL "stable")
endif()
message(STATUS "TuxBlox launcher version: ${TUXBLOX_LAUNCHER_VERSION}")
message(STATUS "TuxBlox launcher channel: ${TUXBLOX_LAUNCHER_CHANNEL}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

add_library(launcher_core STATIC
    src/app.cpp
    src/discord_rpc.cpp
    src/roblox_place_info.cpp
    src/studio_presence.cpp
    src/install_paths.cpp
    src/root_guard.cpp
    src/prefix_user.cpp
    src/checksum.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/manifest.cpp
    src/lnk_resolver.cpp
    src/launch_paths.cpp
    src/process_launcher.cpp
    src/updater.cpp
    src/headless_launch.cpp
    src/desktop_integration.cpp
    src/desktop_notify.cpp
    src/copyright_file.cpp
    src/license_file.cpp
    src/settings.cpp
    src/fastflag_file.cpp
    src/fastflag_import.cpp
    src/versions_manifest.cpp
    src/crash_report.cpp
    src/container_env.cpp
    src/single_instance.cpp
    src/prefix_session.cpp
    src/app_scope.cpp
    src/system_info.cpp
    src/pe_version.cpp
    src/process_name.cpp
    src/system_requirements.cpp
    src/roblox_log_capture.cpp
    src/roblox_deploy.cpp
    src/roblox_autoupdate.cpp
    src/prefix_file_bridge.cpp
    src/wine_shortcut_export.cpp
    src/open_url.cpp
    ${TUXBLOX_UI_SRC}/adw_env.cpp
)
if(TUXBLOX_LIBADWAITA_COMMIT)
    target_compile_definitions(launcher_core PRIVATE TUXBLOX_LIBADWAITA_COMMIT="${TUXBLOX_LIBADWAITA_COMMIT}")
endif()

target_include_directories(launcher_core PUBLIC
    src third_party ${CMAKE_BINARY_DIR}/generated ${TUXBLOX_UI_SRC}
    ${TUXBLOX_SHARED_INCLUDE})
target_link_libraries(launcher_core PUBLIC
    ${TUXBLOX_SHARED_LIBS} ${CMAKE_DL_LIBS} Threads::Threads)
# --exclude-libs keeps these archives' symbols out of the binary's dynamic symbol table. They are
# still linked in and callable from our own code; this only controls whether they are exported.
# It is load-bearing for zlib, which this process holds twice on purpose: a static copy inside
# libarchive.a here, and the interface stack's own libz.so.1, which its libtiff and libfreetype
# both load. Exported, our static inflate and crc32 would be handed to them instead of the
# stack's. A SIGSEGV in EC_GROUP_order_bits from exactly this mechanism, when a static OpenSSL
# 1.1 met a machine's libcurl built against OpenSSL 3, is why the flag was first added.
target_link_options(launcher_core PUBLIC
    -Wl,--exclude-libs=libcurl.a -Wl,--exclude-libs=libnghttp2.a
    -Wl,--exclude-libs=libarchive.a -Wl,--exclude-libs=libzstd.a
    -Wl,--exclude-libs=libz.a
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)

include(cmake/FetchLogo.cmake)
include(cmake/FetchWindowIcon.cmake)

include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)
include(cmake/EmbedLicense.cmake)
include(cmake/EmbedAssets.cmake)

# launcher_core's own sources need these generated headers regardless of
# which downstream target (TuxBloxLauncher or launcher_ui_adw) triggers its
# build: desktop_integration.cpp embeds the logo PNG bytes directly (it
# writes a standalone .desktop icon file),
# and copyright_file.cpp/license_file.cpp embed license text. Declared here
# on launcher_core itself, not duplicated on every consumer, since these
# are launcher_core's own compile-time requirements.
add_dependencies(launcher_core generate_logo_header generate_font_header generate_thirdparty_license_headers generate_license_header)

# The window needs the interface stack's development tree, which only exists once shared/tools/ui-build.sh has run; without it only launcher_core and its tests build, and build.sh refuses to finish.
if(TUXBLOX_UI_STACK_DEV)
    include(${TUXBLOX_UI_SRC}/UiStack.cmake)

    add_library(launcher_ui_adw STATIC
        src/ui_adw/widgets.cpp
        src/ui_adw/message_box.cpp
        src/ui_adw/launcher_window.cpp
        src/ui_adw/page_home.cpp
        src/ui_adw/page_settings.cpp
        src/ui_adw/page_versions.cpp
        src/ui_adw/page_fastflags.cpp
        src/ui_adw/page_about.cpp
        src/ui_adw/versions_tab_state.cpp
        src/ui_adw/home_card_state.cpp
        ${TUXBLOX_UI_SRC}/adw_look.cpp
    )
    target_include_directories(launcher_ui_adw PUBLIC
        src src/ui_adw ${CMAKE_BINARY_DIR}/generated ${ADWAITA_INCLUDE_DIRS})
    target_compile_options(launcher_ui_adw PUBLIC ${ADWAITA_CFLAGS_OTHER})
    target_link_libraries(launcher_ui_adw PUBLIC launcher_core ${ADWAITA_LINK_LIBRARIES})
    add_dependencies(launcher_ui_adw generate_asset_headers generate_logo_header)

    add_executable(TuxBloxLauncher
        src/main.cpp
        src/watch_launch.cpp
    )
    target_include_directories(TuxBloxLauncher PRIVATE ${CMAKE_BINARY_DIR}/generated)
    target_link_libraries(TuxBloxLauncher PRIVATE launcher_core launcher_ui_adw ${CMAKE_DL_LIBS} Threads::Threads)
    # The interface libraries are installed in the libtuxblox folder beside this binary
    set_target_properties(TuxBloxLauncher PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN/libtuxblox/lib/x86_64-linux-gnu:$ORIGIN/libtuxblox/lib")
    add_dependencies(TuxBloxLauncher generate_thirdparty_license_headers generate_license_header generate_font_header)
endif()

enable_testing()
add_subdirectory(tests)
