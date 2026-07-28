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
    src/manifest.cpp
    src/downloader.cpp
    src/tar_extract.cpp
    src/checksum.cpp
    src/install_paths.cpp
    src/progress.cpp
    src/installer_steps.cpp
)
target_include_directories(installer_core PUBLIC
    src third_party
    ${CURL_INCLUDE_DIRS} ${ARCHIVE_INCLUDE_DIRS} ${OPENSSL_INCLUDE_DIRS})
target_link_libraries(installer_core PUBLIC
    ${CURL_LIBRARIES} ${ARCHIVE_LIBRARIES} ${OPENSSL_LIBRARIES})
target_compile_options(installer_core PUBLIC
    ${CURL_CFLAGS_OTHER} ${ARCHIVE_CFLAGS_OTHER} ${OPENSSL_CFLAGS_OTHER})

include(cmake/FetchLogo.cmake)
include(cmake/FetchFont.cmake)
include(cmake/EmbedThirdPartyLicenses.cmake)

add_executable(TuxBloxInstaller
    src/main.cpp
    src/app.cpp
    src/ui.cpp
    src/desktop_shortcut.cpp
    src/copyright_file.cpp
)
target_include_directories(TuxBloxInstaller PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(TuxBloxInstaller PRIVATE installer_core imgui ${SDL2_LIBRARIES} OpenGL::GL)
add_dependencies(TuxBloxInstaller generate_logo_header generate_font_header generate_thirdparty_license_headers)

enable_testing()
add_subdirectory(tests)
