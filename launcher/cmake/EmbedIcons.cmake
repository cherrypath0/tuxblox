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

# Embeds the bundled icon PNGs (launcher/assets/) as C headers via
# BinToHeader.cmake -- same shape as FetchWindowIcon.cmake, minus the
# download step, since these are vendored source files rather than
# fetched from a live URL at build time (see
# docs/superpowers/specs/2026-07-28-launcher-design.md, "Bundled assets").
# No rasterization step needed -- these are hand-authored PNGs, not SVGs.

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

# Four parallel lists, one entry per icon (kept parallel rather than
# packed tuples -- CMake lists are just semicolon-separated strings, so a
# "list of lists" doesn't nest cleanly).
set(ICON_NAMES       player          studio          home       info       globe       docs       github       discord      settings)
set(ICON_BASENAMES   roblox_player_icon roblox_studio_icon icon_home  icon_info  icon_globe  icon_docs  icon_github  icon_discord icon_settings)
set(ICON_SOURCE_PNGS roblox-player.png roblox-studio.png home.png  info.png   globe.png   docs.png   github.png   discord.png  settings.png)
set(ICON_SYMBOLS     kRobloxPlayerIconPng kRobloxStudioIconPng kIconHomePng kIconInfoPng kIconGlobePng kIconDocsPng kIconGithubPng kIconDiscordPng kIconSettingsPng)

list(LENGTH ICON_NAMES ICON_COUNT)
math(EXPR ICON_LAST_INDEX "${ICON_COUNT} - 1")

set(ICON_HEADERS "")
foreach(i RANGE ${ICON_LAST_INDEX})
    list(GET ICON_NAMES ${i} icon_name)
    list(GET ICON_BASENAMES ${i} icon_basename)
    list(GET ICON_SOURCE_PNGS ${i} icon_source_png)
    list(GET ICON_SYMBOLS ${i} icon_symbol)

    set(icon_png "${CMAKE_SOURCE_DIR}/assets/${icon_source_png}")
    set(icon_header "${GENERATED_DIR}/${icon_basename}_png.h")

    add_custom_command(
        OUTPUT ${icon_header}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${icon_png} -DOUTPUT=${icon_header}
                -DSYMBOL=${icon_symbol}
                -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
        DEPENDS ${icon_png}
        COMMENT "Embedding the ${icon_name} icon"
        VERBATIM
    )
    list(APPEND ICON_HEADERS ${icon_header})
endforeach()

add_custom_target(generate_icon_headers DEPENDS ${ICON_HEADERS})
