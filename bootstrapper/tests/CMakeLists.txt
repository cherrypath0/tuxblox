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

# Every test here checks correctness with plain assert(). Release builds
# define NDEBUG, which compiles assert() away and would turn this whole
# directory into "always passes" -- -UNDEBUG keeps them live without
# changing the optimisation level of anything that ships.
add_compile_options(-UNDEBUG)

add_executable(test_app test_app.cpp)
target_link_libraries(test_app PRIVATE bootstrapper_core)
add_test(NAME app COMMAND test_app)

add_executable(test_cli test_cli.cpp)
target_link_libraries(test_cli PRIVATE bootstrapper_core)
add_test(NAME cli COMMAND test_cli)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config PRIVATE bootstrapper_core)
add_test(NAME config COMMAND test_config)

add_executable(test_progress test_progress.cpp)
target_link_libraries(test_progress PRIVATE bootstrapper_core)
add_test(NAME progress COMMAND test_progress)

add_executable(test_ui_scale test_ui_scale.cpp)
target_link_libraries(test_ui_scale PRIVATE bootstrapper_core)
add_test(NAME ui_scale COMMAND test_ui_scale)

add_executable(test_install_plan test_install_plan.cpp)
target_link_libraries(test_install_plan PRIVATE bootstrapper_core)
add_test(NAME install_plan COMMAND test_install_plan)

add_executable(test_roblox_deploy test_roblox_deploy.cpp)
target_link_libraries(test_roblox_deploy PRIVATE bootstrapper_core)
add_test(NAME roblox_deploy COMMAND test_roblox_deploy)

add_executable(test_downloader test_downloader.cpp)
target_link_libraries(test_downloader PRIVATE bootstrapper_core)
add_test(NAME downloader COMMAND test_downloader)

add_executable(test_tar_extract test_tar_extract.cpp)
target_link_libraries(test_tar_extract PRIVATE bootstrapper_core)
add_test(NAME tar_extract COMMAND test_tar_extract)

add_executable(test_checksum test_checksum.cpp)
target_link_libraries(test_checksum PRIVATE bootstrapper_core)
add_test(NAME checksum COMMAND test_checksum)
