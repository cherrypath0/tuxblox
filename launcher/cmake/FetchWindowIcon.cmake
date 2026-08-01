# Downloads the pre-rendered TuxBlox window/taskbar icon PNG and embeds it
# as a C header. No rasterization step needed (unlike FetchLogo.cmake's
# logo, this is already a PNG, and a dedicated icon-sized export rather
# than the same svg used for the big in-app logo) -- runs at build time so
# the compiled launcher bundles the icon without needing network access
# itself.

set(WINDOW_ICON_PNG_URL "https://assetdelivery.tuxblox.net/images/png/icon/tuxblox-medium.png")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(WINDOW_ICON_PNG_PATH "${GENERATED_DIR}/tuxblox_window_icon.png")
set(WINDOW_ICON_HEADER_PATH "${GENERATED_DIR}/tuxblox_window_icon_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${WINDOW_ICON_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${WINDOW_ICON_PNG_URL} -DDEST=${WINDOW_ICON_PNG_PATH}
            -DUSERAGENT=TuxBlox-Client/1.0
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${WINDOW_ICON_PNG_PATH} -DOUTPUT=${WINDOW_ICON_HEADER_PATH}
            -DSYMBOL=kTuxbloxWindowIconPng
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding TuxBlox window icon"
    VERBATIM
)

add_custom_target(generate_window_icon_header DEPENDS ${WINDOW_ICON_HEADER_PATH})
