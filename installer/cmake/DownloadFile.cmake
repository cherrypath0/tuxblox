# Usage: cmake -DURL=... -DDEST=... [-DUSERAGENT=...] -P DownloadFile.cmake
if(NOT DEFINED URL OR NOT DEFINED DEST)
    message(FATAL_ERROR "DownloadFile.cmake requires -DURL and -DDEST")
endif()

if(DEFINED USERAGENT)
    file(DOWNLOAD "${URL}" "${DEST}" STATUS status LOG log USERAGENT "${USERAGENT}")
else()
    file(DOWNLOAD "${URL}" "${DEST}" STATUS status LOG log)
endif()
list(GET status 0 statusCode)
if(NOT statusCode EQUAL 0)
    message(FATAL_ERROR "Failed to download ${URL}: ${log}")
endif()
