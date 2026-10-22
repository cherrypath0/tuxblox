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

# The launcher's icons, embedded as byte arrays so the binary needs no icon theme and no files beside it

set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
file(MAKE_DIRECTORY ${GENERATED_DIR})

function(embedAsset name headerName symbol)
    # Defaults to png so anything that must stay a bitmap -- the logo, which is written to disk as a file -- needs nothing said about it
    set(extension "png")
    if(ARGC GREATER 3)
        set(extension "${ARGV3}")
    endif()
    set(input "${CMAKE_SOURCE_DIR}/assets/${name}.${extension}")
    set(output "${GENERATED_DIR}/${headerName}")
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${input} -DOUTPUT=${output} -DSYMBOL=${symbol}
                -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
        DEPENDS ${input}
        COMMENT "Embedding ${name}.${extension}"
        VERBATIM
    )
    set_property(GLOBAL APPEND PROPERTY LAUNCHER_ASSET_HEADERS ${output})
endfunction()

embedAsset(home asset_home.h kAssetHome svg)
embedAsset(info asset_info.h kAssetInfo svg)
embedAsset(globe asset_globe.h kAssetGlobe svg)
embedAsset(docs asset_docs.h kAssetDocs svg)
embedAsset(github asset_github.h kAssetGithub svg)
embedAsset(discord asset_discord.h kAssetDiscord svg)
embedAsset(settings asset_settings.h kAssetSettings svg)
embedAsset(privacy asset_privacy.h kAssetPrivacy svg)
embedAsset(scale asset_scale.h kAssetScale svg)
embedAsset(download asset_download.h kAssetDownload svg)
embedAsset(play asset_play.h kAssetPlay svg)
embedAsset(stop asset_stop.h kAssetStop svg)
embedAsset(tuxblox-icon asset_tuxblox_icon.h kAssetTuxbloxIcon)
embedAsset(fastflags asset_fastflags.h kAssetFastflags svg)
embedAsset(roblox-rdd asset_roblox_rdd.h kAssetRobloxRdd svg)
embedAsset(roblox-player asset_roblox_player.h kAssetRobloxPlayer svg)
embedAsset(roblox-studio asset_roblox_studio.h kAssetRobloxStudio svg)
embedAsset(sessions asset_sessions.h kAssetSessions svg)
embedAsset(follow asset_follow.h kAssetFollow svg)
embedAsset(wrap asset_wrap.h kAssetWrap svg)
embedAsset(copy asset_copy.h kAssetCopy svg)
embedAsset(save asset_save.h kAssetSave svg)
embedAsset(folder asset_folder.h kAssetFolder svg)
embedAsset(search asset_search.h kAssetSearch svg)
embedAsset(close asset_close.h kAssetClose svg)
embedAsset(chevron-up asset_chevron_up.h kAssetChevronUp svg)
embedAsset(chevron-down asset_chevron_down.h kAssetChevronDown svg)

get_property(LAUNCHER_ASSET_HEADERS GLOBAL PROPERTY LAUNCHER_ASSET_HEADERS)
add_custom_target(generate_asset_headers DEPENDS ${LAUNCHER_ASSET_HEADERS})
