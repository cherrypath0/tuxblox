# Downloads the TuxBlox logo svg, rasterizes it to PNG via rsvg-convert,
# then generates a C header embedding the PNG bytes. Runs at build time so
# the compiled installer bundles the logo without needing network access
# itself -- only the build machine needs it.

find_program(RSVG_CONVERT rsvg-convert)
if(NOT RSVG_CONVERT)
    message(FATAL_ERROR "rsvg-convert not found. Install librsvg (e.g. 'librsvg2-bin' on Debian/Ubuntu, 'librsvg2-tools' on Fedora, 'librsvg' on Arch/Homebrew) and re-run cmake.")
endif()

set(LOGO_SVG_URL "https://assetdelivery.tuxblox.net/images/svg/tuxblox.svg")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(LOGO_SVG_PATH "${GENERATED_DIR}/tuxblox_logo.svg")
set(LOGO_PNG_PATH "${GENERATED_DIR}/tuxblox_logo.png")
set(LOGO_HEADER_PATH "${GENERATED_DIR}/tuxblox_logo_png.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${LOGO_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${LOGO_SVG_URL} -DDEST=${LOGO_SVG_PATH}
            -DUSERAGENT=TuxBlox-Client/1.0
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${RSVG_CONVERT} -w 256 -h 256 -o ${LOGO_PNG_PATH} ${LOGO_SVG_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${LOGO_PNG_PATH} -DOUTPUT=${LOGO_HEADER_PATH}
            -DSYMBOL=kTuxbloxLogoPng
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding TuxBlox logo"
    VERBATIM
)

add_custom_target(generate_logo_header DEPENDS ${LOGO_HEADER_PATH})
