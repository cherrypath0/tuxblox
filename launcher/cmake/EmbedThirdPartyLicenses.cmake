# Embeds license texts for third-party dependencies compiled directly into
# the launcher binary (Dear ImGui, nlohmann/json), so copyright_file.cpp
# can write proper attribution into COPYRIGHT.txt. Identical to
# installer/cmake/EmbedThirdPartyLicenses.cmake.

set(IMGUI_LICENSE_PATH "${CMAKE_SOURCE_DIR}/third_party/imgui/LICENSE.txt")
set(JSON_LICENSE_URL "https://raw.githubusercontent.com/nlohmann/json/v3.11.3/LICENSE.MIT")
set(GENERATED_DIR "${CMAKE_BINARY_DIR}/generated")
set(JSON_LICENSE_TXT_PATH "${GENERATED_DIR}/json-LICENSE.MIT")
set(IMGUI_LICENSE_HEADER_PATH "${GENERATED_DIR}/imgui_license_txt.h")
set(JSON_LICENSE_HEADER_PATH "${GENERATED_DIR}/json_license_txt.h")

file(MAKE_DIRECTORY ${GENERATED_DIR})

if(NOT EXISTS ${IMGUI_LICENSE_PATH})
    message(FATAL_ERROR "Dear ImGui LICENSE.txt not found at ${IMGUI_LICENSE_PATH} -- run ./vendor.sh first.")
endif()

add_custom_command(
    OUTPUT ${IMGUI_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DINPUT=${IMGUI_LICENSE_PATH} -DOUTPUT=${IMGUI_LICENSE_HEADER_PATH}
            -DSYMBOL=kImguiLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    DEPENDS ${IMGUI_LICENSE_PATH}
    COMMENT "Embedding Dear ImGui license text"
    VERBATIM
)

add_custom_command(
    OUTPUT ${JSON_LICENSE_HEADER_PATH}
    COMMAND ${CMAKE_COMMAND} -DURL=${JSON_LICENSE_URL} -DDEST=${JSON_LICENSE_TXT_PATH}
            -P ${CMAKE_SOURCE_DIR}/cmake/DownloadFile.cmake
    COMMAND ${CMAKE_COMMAND} -DINPUT=${JSON_LICENSE_TXT_PATH} -DOUTPUT=${JSON_LICENSE_HEADER_PATH}
            -DSYMBOL=kJsonLicenseTxt
            -P ${CMAKE_SOURCE_DIR}/cmake/BinToHeader.cmake
    COMMENT "Fetching and embedding nlohmann/json license text"
    VERBATIM
)

add_custom_target(generate_thirdparty_license_headers DEPENDS
    ${IMGUI_LICENSE_HEADER_PATH} ${JSON_LICENSE_HEADER_PATH})
