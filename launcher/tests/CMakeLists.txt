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
# CMAKE_BUILD_TYPE=Release (what launcher/build.sh always configures --
# see its cmake invocation) defines -DNDEBUG, which compiles assert() to a
# no-op -- silently turning every test in this suite into "always passes"
# regardless of CMAKE_BUILD_TYPE. -UNDEBUG here, scoped to this directory
# only, keeps assert() active in every test executable without touching
# the optimization level or NDEBUG status of launcher_core or the shipped
# TuxBloxLauncher binary (both defined in the parent CMakeLists.txt,
# outside this directory's scope).
add_compile_options(-UNDEBUG)

add_executable(test_install_paths test_install_paths.cpp)
target_link_libraries(test_install_paths PRIVATE launcher_core)
add_test(NAME install_paths COMMAND test_install_paths)

add_executable(test_checksum test_checksum.cpp)
target_link_libraries(test_checksum PRIVATE launcher_core)
add_test(NAME checksum COMMAND test_checksum)

add_executable(test_downloader test_downloader.cpp)
target_link_libraries(test_downloader PRIVATE launcher_core)
add_test(NAME downloader COMMAND test_downloader)

add_executable(test_tar_extract test_tar_extract.cpp)
target_link_libraries(test_tar_extract PRIVATE launcher_core)
add_test(NAME tar_extract COMMAND test_tar_extract)

add_executable(test_manifest test_manifest.cpp)
target_link_libraries(test_manifest PRIVATE launcher_core)
add_test(NAME manifest COMMAND test_manifest)

add_executable(test_lnk_resolver test_lnk_resolver.cpp)
target_link_libraries(test_lnk_resolver PRIVATE launcher_core)
add_test(NAME lnk_resolver COMMAND test_lnk_resolver)

add_executable(test_process_launcher test_process_launcher.cpp)
target_link_libraries(test_process_launcher PRIVATE launcher_core)
add_test(NAME process_launcher COMMAND test_process_launcher)

add_executable(test_updater test_updater.cpp)
target_link_libraries(test_updater PRIVATE launcher_core)
add_test(NAME updater COMMAND test_updater)

add_executable(test_settings test_settings.cpp)
target_link_libraries(test_settings PRIVATE launcher_core)
add_test(NAME settings COMMAND test_settings)
