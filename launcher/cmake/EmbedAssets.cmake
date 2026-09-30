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
    set(input "${CMAKE_SOURCE_DIR}/assets/${name}.png")
    set(output "${GENERATED_DIR}/${headerName}")
    add_custom_command(
        OUTPUT ${output}
        COMMAND ${CMAKE_COMMAND} -DINPUT=${input} -DOUTPUT=${output} -DSYMBOL=${symbol}
                -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
        DEPENDS ${input}
        COMMENT "Embedding ${name}.png"
        VERBATIM
    )
    set_property(GLOBAL APPEND PROPERTY LAUNCHER_ASSET_HEADERS ${output})
endfunction()

embedAsset(home asset_home.h kAssetHome)
embedAsset(info asset_info.h kAssetInfo)
embedAsset(globe asset_globe.h kAssetGlobe)
embedAsset(docs asset_docs.h kAssetDocs)
embedAsset(github asset_github.h kAssetGithub)
embedAsset(discord asset_discord.h kAssetDiscord)
embedAsset(settings asset_settings.h kAssetSettings)
embedAsset(privacy asset_privacy.h kAssetPrivacy)
embedAsset(download asset_download.h kAssetDownload)
embedAsset(fastflags asset_fastflags.h kAssetFastflags)
embedAsset(roblox-rdd asset_roblox_rdd.h kAssetRobloxRdd)
embedAsset(roblox-player asset_roblox_player.h kAssetRobloxPlayer)
embedAsset(roblox-studio asset_roblox_studio.h kAssetRobloxStudio)

get_property(LAUNCHER_ASSET_HEADERS GLOBAL PROPERTY LAUNCHER_ASSET_HEADERS)
add_custom_target(generate_asset_headers DEPENDS ${LAUNCHER_ASSET_HEADERS})
