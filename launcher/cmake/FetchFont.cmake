# Downloads two static-weight Inter TTFs (OFL-1.1) from jsDelivr's fontsource
# mirror, pinned to a fixed release, and generates C headers embedding the
# raw TTF bytes, plus the OFL license text -- identical to
# installer/cmake/FetchFont.cmake.

set(INTER_VERSION "5.0.0")
set(INTER_REGULAR_URL "https://cdn.jsdelivr.net/fontsource/fonts/inter@${INTER_VERSION}/latin-400-normal.ttf")
set(INTER_SEMIBOLD_URL "https://cdn.jsdelivr.net/fontsource/fonts/inter@${INTER_VERSION}/latin-600-normal.ttf")
set(INTER_OFL_URL "https://raw.githubusercontent.com/google/fonts/main/ofl/inter/OFL.txt")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(INTER_REGULAR_TTF_PATH "${GENERATED_DIR}/Inter-Regular.ttf")
set(INTER_SEMIBOLD_TTF_PATH "${GENERATED_DIR}/Inter-SemiBold.ttf")
set(INTER_OFL_TXT_PATH "${GENERATED_DIR}/Inter-OFL.txt")
set(INTER_REGULAR_HEADER_PATH "${GENERATED_DIR}/inter_regular_ttf.h")
set(INTER_SEMIBOLD_HEADER_PATH "${GENERATED_DIR}/inter_semibold_ttf.h")
set(INTER_OFL_HEADER_PATH "${GENERATED_DIR}/inter_ofl_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

add_custom_command(
    OUTPUT ${INTER_REGULAR_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_REGULAR_URL} -DDEST=${INTER_REGULAR_TTF_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_REGULAR_TTF_PATH} -DOUTPUT=${INTER_REGULAR_HEADER_PATH}
            -DSYMBOL=kInterRegularTtf
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter Regular"
    VERBATIM
)

add_custom_command(
    OUTPUT ${INTER_SEMIBOLD_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_SEMIBOLD_URL} -DDEST=${INTER_SEMIBOLD_TTF_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_SEMIBOLD_TTF_PATH} -DOUTPUT=${INTER_SEMIBOLD_HEADER_PATH}
            -DSYMBOL=kInterSemiBoldTtf
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter SemiBold"
    VERBATIM
)

add_custom_command(
    OUTPUT ${INTER_OFL_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${INTER_OFL_URL} -DDEST=${INTER_OFL_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${INTER_OFL_TXT_PATH} -DOUTPUT=${INTER_OFL_HEADER_PATH}
            -DSYMBOL=kInterOflLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding Inter OFL license text"
    VERBATIM
)

add_custom_target(generate_font_header DEPENDS
    ${INTER_REGULAR_HEADER_PATH} ${INTER_SEMIBOLD_HEADER_PATH} ${INTER_OFL_HEADER_PATH})
