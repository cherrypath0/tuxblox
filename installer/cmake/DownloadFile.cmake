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

# Usage: cmake -DURL=... -DDEST=... [-DUSERAGENT=...] -P DownloadFile.cmake
if(NOT DEFINED URL OR NOT DEFINED DEST)
    message(FATAL_ERROR "DownloadFile.cmake requires -DURL and -DDEST")
endif()

# TLS_VERIFY is off by default below CMake 3.31 and the builder runs 3.16, so it is named here. One of
# the things fetched this way is the list of certificates TuxBlox falls back on, and an unverified
# download of that would let whoever answered decide what every build of TuxBlox trusts.
if(DEFINED USERAGENT)
    file(DOWNLOAD "${URL}" "${DEST}" STATUS status LOG log TLS_VERIFY ON USERAGENT "${USERAGENT}")
else()
    file(DOWNLOAD "${URL}" "${DEST}" STATUS status LOG log TLS_VERIFY ON)
endif()
list(GET status 0 statusCode)
if(NOT statusCode EQUAL 0)
    message(FATAL_ERROR "Failed to download ${URL}: ${log}")
endif()
