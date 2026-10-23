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

add_executable(test_lnk_resolver test_lnk_resolver.cpp)
target_link_libraries(test_lnk_resolver PRIVATE launcher_core)
add_test(NAME lnk_resolver COMMAND test_lnk_resolver)

add_executable(test_process_launcher test_process_launcher.cpp)
target_link_libraries(test_process_launcher PRIVATE launcher_core)
add_test(NAME process_launcher COMMAND test_process_launcher)

add_executable(test_update_decision test_update_decision.cpp)
target_link_libraries(test_update_decision PRIVATE launcher_core)
add_test(NAME update_decision COMMAND test_update_decision)

add_executable(test_updater test_updater.cpp)
target_link_libraries(test_updater PRIVATE launcher_core)
add_test(NAME updater COMMAND test_updater)

add_executable(test_settings test_settings.cpp)
target_link_libraries(test_settings PRIVATE launcher_core)
add_test(NAME settings COMMAND test_settings)

add_executable(test_fastflag_file test_fastflag_file.cpp)
target_link_libraries(test_fastflag_file PRIVATE launcher_core)
add_test(NAME fastflag_file COMMAND test_fastflag_file)

add_executable(test_container_env test_container_env.cpp)
target_link_libraries(test_container_env PRIVATE launcher_core)
add_test(NAME container_env COMMAND test_container_env)

add_executable(test_single_instance test_single_instance.cpp)
target_link_libraries(test_single_instance PRIVATE launcher_core)
add_test(NAME single_instance COMMAND test_single_instance)

add_executable(test_app_scope test_app_scope.cpp)
target_link_libraries(test_app_scope PRIVATE launcher_core)
add_test(NAME app_scope COMMAND test_app_scope)

add_executable(test_license_file test_license_file.cpp)
target_link_libraries(test_license_file PRIVATE launcher_core)
add_test(NAME license_file COMMAND test_license_file)

add_executable(test_system_info test_system_info.cpp)
target_link_libraries(test_system_info PRIVATE launcher_core)
add_test(NAME system_info COMMAND test_system_info)

add_executable(test_pe_version test_pe_version.cpp)
target_link_libraries(test_pe_version PRIVATE launcher_core)
add_test(NAME pe_version COMMAND test_pe_version)

add_executable(test_system_requirements test_system_requirements.cpp)
target_link_libraries(test_system_requirements PRIVATE launcher_core)
add_test(NAME system_requirements COMMAND test_system_requirements)

add_executable(test_desktop_integration test_desktop_integration.cpp)
target_link_libraries(test_desktop_integration PRIVATE launcher_core)
add_test(NAME desktop_integration COMMAND test_desktop_integration)

add_executable(test_process_name test_process_name.cpp)
target_link_libraries(test_process_name PRIVATE launcher_core)
add_test(NAME process_name COMMAND test_process_name)

add_executable(test_roblox_log_capture test_roblox_log_capture.cpp)
target_link_libraries(test_roblox_log_capture PRIVATE launcher_core)
add_test(NAME roblox_log_capture COMMAND test_roblox_log_capture)

add_executable(test_crash_report test_crash_report.cpp)
target_link_libraries(test_crash_report PRIVATE launcher_core)
add_test(NAME crash_report COMMAND test_crash_report)

add_executable(test_versions_manifest test_versions_manifest.cpp)
target_link_libraries(test_versions_manifest PRIVATE launcher_core)
add_test(NAME versions_manifest COMMAND test_versions_manifest)

add_executable(test_roblox_deploy test_roblox_deploy.cpp)
target_link_libraries(test_roblox_deploy PRIVATE launcher_core)
add_test(NAME roblox_deploy COMMAND test_roblox_deploy)

add_executable(test_versions_tab_state test_versions_tab_state.cpp ../src/ui_adw/versions_tab_state.cpp)
target_link_libraries(test_versions_tab_state PRIVATE launcher_core)
add_test(NAME versions_tab_state COMMAND test_versions_tab_state)

add_executable(test_home_card_state test_home_card_state.cpp ../src/ui_adw/home_card_state.cpp)
target_link_libraries(test_home_card_state PRIVATE launcher_core)
add_test(NAME home_card_state COMMAND test_home_card_state)

add_executable(test_prefix_session test_prefix_session.cpp)
target_link_libraries(test_prefix_session PRIVATE launcher_core)
add_test(NAME prefix_session COMMAND test_prefix_session)

add_executable(test_prefix_file_bridge test_prefix_file_bridge.cpp)
target_link_libraries(test_prefix_file_bridge PRIVATE launcher_core)
add_test(NAME prefix_file_bridge COMMAND test_prefix_file_bridge)

add_executable(test_wine_shortcut_export test_wine_shortcut_export.cpp)
target_link_libraries(test_wine_shortcut_export PRIVATE launcher_core)
add_test(NAME wine_shortcut_export COMMAND test_wine_shortcut_export)

add_executable(test_roblox_autoupdate test_roblox_autoupdate.cpp)
target_link_libraries(test_roblox_autoupdate PRIVATE launcher_core)
add_test(NAME roblox_autoupdate COMMAND test_roblox_autoupdate)

add_executable(test_root_guard test_root_guard.cpp)
target_link_libraries(test_root_guard PRIVATE launcher_core)
add_test(NAME root_guard COMMAND test_root_guard)

add_executable(test_prefix_user test_prefix_user.cpp)
target_link_libraries(test_prefix_user PRIVATE launcher_core)
add_test(NAME prefix_user COMMAND test_prefix_user)

add_executable(test_discord_rpc test_discord_rpc.cpp)
target_link_libraries(test_discord_rpc PRIVATE launcher_core)
add_test(NAME discord_rpc COMMAND test_discord_rpc)

add_executable(test_studio_presence test_studio_presence.cpp)
target_link_libraries(test_studio_presence PRIVATE launcher_core)
add_test(NAME studio_presence COMMAND test_studio_presence)

add_executable(test_settings_discord test_settings_discord.cpp)
target_link_libraries(test_settings_discord PRIVATE launcher_core)
add_test(NAME settings_discord COMMAND test_settings_discord)

add_executable(test_settings_theme test_settings_theme.cpp)
target_link_libraries(test_settings_theme PRIVATE launcher_core)
add_test(NAME settings_theme COMMAND test_settings_theme)

add_executable(test_roblox_place_info test_roblox_place_info.cpp)
target_link_libraries(test_roblox_place_info PRIVATE launcher_core)
add_test(NAME roblox_place_info COMMAND test_roblox_place_info)

add_executable(test_child_environment test_child_environment.cpp)
target_link_libraries(test_child_environment PRIVATE launcher_core)
add_test(NAME child_environment COMMAND test_child_environment)

# These need the interface stack, which only the container build with TUXBLOX_UI_STACK_DEV provides
if(TARGET launcher_ui_adw)
    add_executable(test_widgets test_widgets.cpp)
    target_link_libraries(test_widgets PRIVATE launcher_ui_adw)
    target_include_directories(test_widgets PRIVATE ${CMAKE_BINARY_DIR}/generated)
    add_dependencies(test_widgets generate_asset_headers)
    add_test(NAME widgets COMMAND test_widgets)

    add_executable(test_message_box test_message_box.cpp)
    target_link_libraries(test_message_box PRIVATE launcher_ui_adw)
    add_test(NAME message_box COMMAND test_message_box)
endif()

add_executable(test_desktop_notify test_desktop_notify.cpp)
target_link_libraries(test_desktop_notify PRIVATE launcher_core)
add_test(NAME desktop_notify COMMAND test_desktop_notify)

add_executable(test_fastflag_import test_fastflag_import.cpp)
target_link_libraries(test_fastflag_import PRIVATE launcher_core)
add_test(NAME fastflag_import COMMAND test_fastflag_import)

add_executable(test_release_file ${TUXBLOX_SHARED_RELEASE}/../tests/test_release_file.cpp)
target_link_libraries(test_release_file PRIVATE launcher_core)
add_test(NAME release_file COMMAND test_release_file)

add_executable(test_ca_bundle ${TUXBLOX_SHARED_CRYPTO}/../tests/test_ca_bundle.cpp)
target_link_libraries(test_ca_bundle PRIVATE launcher_core)
add_test(NAME ca_bundle COMMAND test_ca_bundle)

add_executable(test_versions_refresh test_versions_refresh.cpp)
target_link_libraries(test_versions_refresh PRIVATE launcher_core)
add_test(NAME versions_refresh COMMAND test_versions_refresh)

add_executable(test_bloxstrap_rpc test_bloxstrap_rpc.cpp)
target_link_libraries(test_bloxstrap_rpc PRIVATE launcher_core)
add_test(NAME bloxstrap_rpc COMMAND test_bloxstrap_rpc)

add_executable(test_player_presence test_player_presence.cpp)
target_link_libraries(test_player_presence PRIVATE launcher_core)
add_test(NAME player_presence COMMAND test_player_presence)

add_executable(test_presence_activity_json test_presence_activity_json.cpp)
target_link_libraries(test_presence_activity_json PRIVATE launcher_core)
add_test(NAME presence_activity_json COMMAND test_presence_activity_json)

add_executable(test_server_region test_server_region.cpp)
target_link_libraries(test_server_region PRIVATE launcher_core)
add_test(NAME server_region COMMAND test_server_region)

add_executable(test_roblox_universe test_roblox_universe.cpp)
target_link_libraries(test_roblox_universe PRIVATE launcher_core)
add_test(NAME roblox_universe COMMAND test_roblox_universe)

add_executable(test_presence_loop test_presence_loop.cpp)
target_link_libraries(test_presence_loop PRIVATE launcher_core)
add_test(NAME presence_loop COMMAND test_presence_loop)

add_executable(test_bootstrap_app test_bootstrap_app.cpp)
target_link_libraries(test_bootstrap_app PRIVATE launcher_core)
add_test(NAME bootstrap_app COMMAND test_bootstrap_app)

add_executable(test_bootstrap_cli test_bootstrap_cli.cpp)
target_link_libraries(test_bootstrap_cli PRIVATE launcher_core)
add_test(NAME bootstrap_cli COMMAND test_bootstrap_cli)

add_executable(test_bootstrap_config test_bootstrap_config.cpp)
target_link_libraries(test_bootstrap_config PRIVATE launcher_core)
add_test(NAME bootstrap_config COMMAND test_bootstrap_config)

add_executable(test_bootstrap_progress test_bootstrap_progress.cpp)
target_link_libraries(test_bootstrap_progress PRIVATE launcher_core)
add_test(NAME bootstrap_progress COMMAND test_bootstrap_progress)

add_executable(test_bootstrap_install_plan test_bootstrap_install_plan.cpp)
target_link_libraries(test_bootstrap_install_plan PRIVATE launcher_core)
add_test(NAME bootstrap_install_plan COMMAND test_bootstrap_install_plan)

add_executable(test_adw_env test_adw_env.cpp ${TUXBLOX_UI_SRC}/adw_env.cpp)
target_include_directories(test_adw_env PRIVATE ${TUXBLOX_UI_SRC})
add_test(NAME adw_env COMMAND test_adw_env)

add_executable(test_bootstrap_dispatch test_bootstrap_dispatch.cpp)
target_link_libraries(test_bootstrap_dispatch PRIVATE launcher_core)
add_test(NAME bootstrap_dispatch COMMAND test_bootstrap_dispatch)

add_executable(test_svg_raster test_svg_raster.cpp ${TUXBLOX_UI_SRC}/svg_raster.cpp)
target_include_directories(test_svg_raster PRIVATE ${TUXBLOX_UI_SRC} ${CMAKE_SOURCE_DIR}/third_party)
add_test(NAME svg_raster COMMAND test_svg_raster)

add_executable(test_palette test_palette.cpp ${TUXBLOX_UI_SRC}/palette.cpp)
target_include_directories(test_palette PRIVATE ${TUXBLOX_UI_SRC})
add_test(NAME palette COMMAND test_palette)

add_executable(test_embedded_icons test_embedded_icons.cpp ${TUXBLOX_UI_SRC}/svg_raster.cpp)
target_include_directories(test_embedded_icons PRIVATE
    ${TUXBLOX_UI_SRC} ${CMAKE_SOURCE_DIR}/third_party ${CMAKE_BINARY_DIR}/generated)
add_dependencies(test_embedded_icons generate_asset_headers generate_logo_header)
add_test(NAME embedded_icons COMMAND test_embedded_icons)

add_executable(test_session_facts test_session_facts.cpp)
target_link_libraries(test_session_facts PRIVATE launcher_core)
add_test(NAME session_facts COMMAND test_session_facts)

add_executable(test_session_list test_session_list.cpp)
target_link_libraries(test_session_list PRIVATE launcher_core)
add_test(NAME session_list COMMAND test_session_list)

add_executable(test_session_log_match test_session_log_match.cpp)
target_link_libraries(test_session_log_match PRIVATE launcher_core)
add_test(NAME session_log_match COMMAND test_session_log_match)

add_executable(test_log_entry test_log_entry.cpp)
target_link_libraries(test_log_entry PRIVATE launcher_core)
add_test(NAME log_entry COMMAND test_log_entry)

add_executable(test_session_log_reader test_session_log_reader.cpp)
target_link_libraries(test_session_log_reader PRIVATE launcher_core)
add_test(NAME session_log_reader COMMAND test_session_log_reader)

add_executable(test_sessions_page_state test_sessions_page_state.cpp ../src/ui_adw/sessions_page_state.cpp)
target_link_libraries(test_sessions_page_state PRIVATE launcher_core)
add_test(NAME sessions_page_state COMMAND test_sessions_page_state)

add_executable(test_launch_follow_up test_launch_follow_up.cpp)
target_link_libraries(test_launch_follow_up PRIVATE launcher_core)
add_test(NAME launch_follow_up COMMAND test_launch_follow_up)
