// TuxBlox - Linux Compatibility Layer for the Roblox Engine
// Copyright (C) 2026 TuxBlox Developers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#include "copyright_file.h"
#include "inter_ofl_license_txt.h"  // generated at build time
#include "json_license_txt.h"       // generated at build time
#include <fstream>

namespace tuxblox {

namespace {

constexpr const char* kCopyrightIntro =
    "This file contains the complete copyright notices and license texts for all \n"
    "all third-party software components, fonts, and dependencies used within this \n"
    "application. \n"
    "\n"
    "These components are provided under their respective open-source licenses, \n"
    "as detailed below.\n"
    "\n";

constexpr const char* kDivider =
    "================================================================================\n";

constexpr const char* kInterHeading = "Inter (font)\nhttps://github.com/rsms/inter\n\n";
constexpr const char* kJsonHeading = "JSON for Modern C++ (nlohmann/json)\nhttps://github.com/nlohmann/json\n\n";
constexpr const char* kStbHeading = "stb_image.h (stb single-file libraries)\nhttps://github.com/nothings/stb\n\n";

constexpr const char* kStbLicenseTxt =
    "This software is available under 2 licenses -- choose whichever you prefer.\n"
    "------------------------------------------------------------------------------\n"
    "ALTERNATIVE A - MIT License\n"
    "Copyright (c) 2017 Sean Barrett\n"
    "Permission is hereby granted, free of charge, to any person obtaining a copy of\n"
    "this software and associated documentation files (the \"Software\"), to deal in\n"
    "the Software without restriction, including without limitation the rights to\n"
    "use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies\n"
    "of the Software, and to permit persons to whom the Software is furnished to do\n"
    "so, subject to the following conditions:\n"
    "The above copyright notice and this permission notice shall be included in all\n"
    "copies or substantial portions of the Software.\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
    "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
    "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
    "SOFTWARE.\n"
    "------------------------------------------------------------------------------\n"
    "ALTERNATIVE B - Public Domain (www.unlicense.org)\n"
    "This is free and unencumbered software released into the public domain.\n"
    "Anyone is free to copy, modify, publish, use, compile, sell, or distribute this\n"
    "software, either in source code form or as a compiled binary, for any purpose,\n"
    "commercial or non-commercial, and by any means.\n"
    "In jurisdictions that recognize copyright laws, the author or authors of this\n"
    "software dedicate any and all copyright interest in the software to the public\n"
    "domain. We make this dedication for the benefit of the public at large and to\n"
    "the detriment of our heirs and successors. We intend this dedication to be an\n"
    "overt act of relinquishment in perpetuity of all present and future rights to\n"
    "this software under copyright law.\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
    "AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN\n"
    "ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION\n"
    "WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.\n";

constexpr const char* kSimpleIconsHeading =
    "Simple Icons (GitHub, Discord brand marks, Roblox logo)\n"
    "Source: https://simpleicons.org\n\n";

constexpr const char* kSimpleIconsLicenseTxt =
    "The underlying vector files from Simple Icons are released under CC0 1.0 Universal\n"
    "(Public Domain Dedication). The resulting PNG assets are dedicated to the public domain:\n"
    "https://creativecommons.org\n\n"
    "NOTE: The underlying brand marks remain protected by trademark laws.\n"
    "\"GitHub\" is a trademark of Microsoft Corporation;\n"
    "\"Discord\" is a trademark of Discord, Inc.;\n"
    "\"Roblox\" is a trademark of Roblox Corporation.\n\n"
    "DISCLAIMER OF AFFILIATION:\n"
    "This software is an independent, third-party custom launcher.\n"
    "It is not affiliated with, authorized, maintained, sponsored, or endorsed\n"
    "by Roblox Corporation or any of its affiliates. The Roblox logo is used\n"
    "strictly as a functional icon to identify the target game service.\n";

void writeEntry(std::ofstream& file, const char* heading,
                 const unsigned char* text, std::size_t textLen) {
    file << kDivider << "\n" << heading;
    file.write(reinterpret_cast<const char*>(text), static_cast<std::streamsize>(textLen));
    file << "\n";
}

void writeEntry(std::ofstream& file, const char* heading, const char* text) {
    file << kDivider << "\n" << heading << text << "\n";
}

} // namespace

void writeCopyrightFile(const std::string& installDir) {
    try {
        std::ofstream file(installDir + "/COPYRIGHT.txt", std::ios::binary);
        if (!file) return;

        file << kCopyrightIntro;
        writeEntry(file, kInterHeading, kInterOflLicenseTxt, kInterOflLicenseTxtLen);
        writeEntry(file, kJsonHeading, kJsonLicenseTxt, kJsonLicenseTxtLen);
        writeEntry(file, kStbHeading, kStbLicenseTxt);
        writeEntry(file, kSimpleIconsHeading, kSimpleIconsLicenseTxt);
        file << kDivider;
    } catch (...) {
        // Best-effort -- a missing COPYRIGHT.txt must not fail an otherwise
        // successful launch.
    }
}

} // namespace tuxblox
