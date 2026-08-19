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

if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/json.hpp")
    message(FATAL_ERROR "third_party/json.hpp is missing. Run ./vendor.sh first.")
endif()
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/third_party/stb_image.h")
    message(FATAL_ERROR "third_party/stb_image.h is missing. Run ./vendor.sh first.")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL REQUIRED libcurl)
pkg_check_modules(ARCHIVE REQUIRED libarchive)
pkg_check_modules(OPENSSL REQUIRED openssl)
# glibc >= 2.34 merged libpthread/libdl's symbols into libc.so itself, so std::thread
# links "for free" on a newer-glibc host -- but the old-glibc-baseline container this
# project builds inside (see build-container/Containerfile) still needs them explicit.
# Threads::Threads is load-bearing (std::thread is used); ${CMAKE_DL_LIBS} is
# defensive (nothing in this target calls dlopen/dlsym directly, but general
# old-glibc-baseline safety makes it cheap insurance).
find_package(Threads REQUIRED)

# Qt6 for the launcher's UI (launcher/src/ui_qt/) -- see Containerfile for
# how it's provisioned inside the old-glibc-baseline build container
# (aqtinstall, since Ubuntu 20.04 ships no Qt6 apt packages at all).
# CMAKE_PREFIX_PATH is set by that container's ENV; a host build outside the
# container needs the same variable pointed at a local Qt6 install.
find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets Test)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/generated)
# TUXBLOX_BUILD_VERSION is the version the root build.sh prompted for; it wins so
# that one release build stamps the same number into the launcher, the installer
# and proton. The checked-in VERSION file is the fallback, which keeps a
# standalone `launcher/build.sh` run (no root build.sh, so no env var) producing a
# sensibly versioned binary rather than an empty string.
if(NOT "$ENV{TUXBLOX_BUILD_VERSION}" STREQUAL "")
    set(TUXBLOX_LAUNCHER_VERSION "$ENV{TUXBLOX_BUILD_VERSION}")
else()
    file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" TUXBLOX_LAUNCHER_VERSION LIMIT_COUNT 1)
endif()
message(STATUS "TuxBlox launcher version: ${TUXBLOX_LAUNCHER_VERSION}")
configure_file(${CMAKE_SOURCE_DIR}/cmake/version.h.in ${CMAKE_BINARY_DIR}/generated/version.h @ONLY)

add_library(launcher_core STATIC
    src/app.cpp
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
    src/license_file.cpp
    src/settings.cpp
    src/versions_manifest.cpp
    src/crash_report.cpp
    src/container_env.cpp
    src/single_instance.cpp
    src/prefix_session.cpp
    src/app_scope.cpp
    src/system_info.cpp
    src/roblox_log_capture.cpp
    src/roblox_deploy.cpp
)
target_include_directories(launcher_core PUBLIC
    src third_party ${CMAKE_BINARY_DIR}/generated
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
target_link_libraries(launcher_core PUBLIC
    ${CURL_LIBRARIES} ${ARCHIVE_LIBRARIES}
    ${OPENSSL_SSL_STATIC_LIB} ${OPENSSL_CRYPTO_STATIC_LIB} ${CMAKE_DL_LIBS}
    Threads::Threads)
# --exclude-libs keeps libssl.a/libcrypto.a's symbols out of the final
# binary's DYNAMIC symbol table (they're still linked in and callable from
# our own code -- this only controls export visibility, not linkage).
# Without this, our statically-linked OpenSSL 1.1 exports symbols like
# EC_GROUP_order_bits/ENGINE_init globally, which the dynamic linker can
# then hand to libcurl.so.4's own dynamically-loaded libssl.so.3/
# libcrypto.so.3 (OpenSSL 3, on any host where that's what's installed)
# instead of its own -- two different, ABI-incompatible OpenSSL major
# versions calling into each other's functions with each other's struct
# layouts. Confirmed via gdb backtrace: SIGSEGV in EC_GROUP_order_bits
# (resolving to our statically-linked copy, at a main-executable address,
# not a libcrypto.so.3 address) reached via
# App::updateCheckThreadMain -> fetchManifestJson -> libcurl.so.4 ->
# SSL_CTX_new_ex -> ENGINE_init, on the very first update check after
# switching OpenSSL to static linking. PUBLIC so it propagates to every
# executable that links launcher_core (TuxBloxLauncher and the test
# binaries alike), matching how the static libs above already propagate.
target_link_options(launcher_core PUBLIC
    -Wl,--exclude-libs=libssl.a -Wl,--exclude-libs=libcrypto.a)
target_compile_options(launcher_core PUBLIC
    ${CURL_CFLAGS_OTHER} ${ARCHIVE_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})

include(cmake/FetchLogo.cmake)
include(cmake/FetchWindowIcon.cmake)

# launcher.qrc's branding entries need the fetched logo/icon PNGs at a path
# stable regardless of CMAKE_BINARY_DIR's actual name/location -- copy them
# next to the checked-in resources/ directory's own qrc at configure time
# instead of hardcoding a relative path into the build directory.
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/resources/tuxblox_logo.png
    COMMAND ${CMAKE_COMMAND} -E copy ${LOGO_PNG_PATH} ${CMAKE_SOURCE_DIR}/resources/tuxblox_logo.png
    DEPENDS ${LOGO_PNG_PATH}
    COMMENT "Staging logo PNG for launcher.qrc"
)
add_custom_command(
    OUTPUT ${CMAKE_SOURCE_DIR}/resources/tuxblox_window_icon.png
    COMMAND ${CMAKE_COMMAND} -E copy ${WINDOW_ICON_PNG_PATH} ${CMAKE_SOURCE_DIR}/resources/tuxblox_window_icon.png
    DEPENDS ${WINDOW_ICON_PNG_PATH}
    COMMENT "Staging window icon PNG for launcher.qrc"
)
add_custom_target(stage_branding_assets DEPENDS
    ${CMAKE_SOURCE_DIR}/resources/tuxblox_logo.png
    ${CMAKE_SOURCE_DIR}/resources/tuxblox_window_icon.png)

include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)
include(cmake/EmbedLicense.cmake)

# launcher_core's own sources need these generated headers regardless of
# which downstream target (TuxBloxLauncher or launcher_ui_qt) triggers its
# build: desktop_integration.cpp embeds the logo PNG bytes directly (not
# via the Qt resource system -- it writes a standalone .desktop icon file),
# and copyright_file.cpp/license_file.cpp embed license text. Declared here
# on launcher_core itself, not duplicated on every consumer, since these
# are launcher_core's own compile-time requirements.
add_dependencies(launcher_core generate_logo_header generate_font_header generate_thirdparty_license_headers generate_license_header)

add_library(launcher_ui_qt STATIC
    src/ui_qt/theme.cpp
    src/ui_qt/toggle_switch.cpp
    src/ui_qt/danger_button.cpp
    src/ui_qt/update_popup.cpp
    src/ui_qt/icon_utils.cpp
    src/ui_qt/open_url.cpp
    src/ui_qt/sidebar.cpp
    src/ui_qt/start_tab.cpp
    src/ui_qt/settings_tab.cpp
    src/ui_qt/versions_tab.cpp
    src/ui_qt/versions_tab_state.cpp
    src/ui_qt/about_tab.cpp
    src/ui_qt/message_box.cpp
    src/ui_qt/main_window.cpp
    resources/launcher.qrc
)
target_include_directories(launcher_ui_qt PUBLIC src third_party ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(launcher_ui_qt PUBLIC Qt6::Widgets launcher_core)
add_dependencies(launcher_ui_qt generate_logo_asset generate_window_icon_asset stage_branding_assets)

add_executable(TuxBloxLauncher
    src/main.cpp
    src/watch_launch.cpp
)
target_include_directories(TuxBloxLauncher PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxLauncher PRIVATE launcher_core launcher_ui_qt ${CMAKE_DL_LIBS} Threads::Threads)
add_dependencies(TuxBloxLauncher generate_thirdparty_license_headers generate_license_header generate_font_header)

enable_testing()
add_subdirectory(tests)
