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

# Every test in this directory checks correctness with plain assert().
# CMAKE_BUILD_TYPE=Release (what installer/build.sh always configures --
# see its cmake invocation) defines -DNDEBUG, which compiles assert() to a
# no-op -- silently turning every test in this suite into "always passes"
# regardless of CMAKE_BUILD_TYPE. -UNDEBUG here, scoped to this directory
# only, keeps assert() active in every test executable without touching
# the optimization level or NDEBUG status of installer_core or the shipped
# TuxBloxInstaller binary (both defined in the parent CMakeLists.txt,
# outside this directory's scope).
add_compile_options(-UNDEBUG)

add_executable(test_install_paths test_install_paths.cpp)
target_link_libraries(test_install_paths PRIVATE installer_core)
add_test(NAME install_paths COMMAND test_install_paths)

add_executable(test_progress test_progress.cpp)
target_link_libraries(test_progress PRIVATE installer_core)
add_test(NAME progress COMMAND test_progress)

add_executable(test_checksum test_checksum.cpp)
target_link_libraries(test_checksum PRIVATE installer_core)
add_test(NAME checksum COMMAND test_checksum)

add_executable(test_manifest test_manifest.cpp)
target_link_libraries(test_manifest PRIVATE installer_core)
add_test(NAME manifest COMMAND test_manifest)

add_executable(test_downloader test_downloader.cpp)
target_link_libraries(test_downloader PRIVATE installer_core)
add_test(NAME downloader COMMAND test_downloader)

add_executable(test_tar_extract test_tar_extract.cpp)
target_link_libraries(test_tar_extract PRIVATE installer_core)
add_test(NAME tar_extract COMMAND test_tar_extract)

add_executable(test_installer_steps test_installer_steps.cpp)
target_link_libraries(test_installer_steps PRIVATE installer_core)
add_test(NAME installer_steps COMMAND test_installer_steps)

add_executable(test_container_env test_container_env.cpp)
target_link_libraries(test_container_env PRIVATE installer_core)
add_test(NAME container_env COMMAND test_container_env)

# desktop_shortcut.cpp is not part of installer_core (it's compiled straight
# into the TuxBloxInstaller executable), so this target compiles it in
# directly and picks up the generated logo header the same way the executable
# does -- the file embeds kTuxbloxLogoPng at build time.
add_executable(test_desktop_shortcut test_desktop_shortcut.cpp ../src/desktop_shortcut.cpp)
target_include_directories(test_desktop_shortcut PRIVATE ${CMAKE_BINARY_DIR}/generated)
target_link_libraries(test_desktop_shortcut PRIVATE installer_core)
add_dependencies(test_desktop_shortcut generate_logo_header)
add_test(NAME desktop_shortcut COMMAND test_desktop_shortcut)

add_executable(test_uninstall test_uninstall.cpp)
target_link_libraries(test_uninstall PRIVATE installer_core)
add_test(NAME uninstall COMMAND test_uninstall)
