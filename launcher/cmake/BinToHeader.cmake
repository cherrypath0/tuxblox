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
