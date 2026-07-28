# Usage: cmake -DINPUT=... -DOUTPUT=... -DSYMBOL=... -P BinToHeader.cmake
if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR NOT DEFINED SYMBOL)
    message(FATAL_ERROR "BinToHeader.cmake requires -DINPUT, -DOUTPUT and -DSYMBOL")
endif()

file(READ "${INPUT}" hexContent HEX)
string(LENGTH "${hexContent}" hexLength)
math(EXPR byteCount "${hexLength} / 2")

string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytesCsv "${hexContent}")

file(WRITE "${OUTPUT}" "// Generated at build time. Do not edit or commit.\n")
file(APPEND "${OUTPUT}" "#pragma once\n")
file(APPEND "${OUTPUT}" "#include <cstddef>\n\n")
file(APPEND "${OUTPUT}" "static const unsigned char ${SYMBOL}[] = {\n${bytesCsv}\n};\n")
file(APPEND "${OUTPUT}" "static const size_t ${SYMBOL}Len = ${byteCount};\n")
