#include "copyright_file.h"
#include "inter_ofl_license_txt.h"  // generated at build time: kInterOflLicenseTxt[], kInterOflLicenseTxtLen
#include "imgui_license_txt.h"      // generated at build time: kImguiLicenseTxt[], kImguiLicenseTxtLen
#include "json_license_txt.h"       // generated at build time: kJsonLicenseTxt[], kJsonLicenseTxtLen
#include <fstream>

namespace tuxblox {

namespace {

constexpr const char* kCopyrightIntro =
    "This file contains the complete copyright notices and license texts for all \n"
    "third-party software components, fonts, and dependencies used within this \n"
    "application. \n"
    "\n"
    "These components are provided under their respective open-source licenses, \n"
    "as detailed below.\n"
    "\n";

constexpr const char* kDivider =
    "================================================================================\n";

constexpr const char* kInterHeading = "Inter (font)\nhttps://github.com/rsms/inter\n\n";
constexpr const char* kImguiHeading = "Dear ImGui\nhttps://github.com/ocornut/imgui\n\n";
constexpr const char* kJsonHeading = "JSON for Modern C++ (nlohmann/json)\nhttps://github.com/nlohmann/json\n\n";
constexpr const char* kStbHeading = "stb_image.h (stb single-file libraries)\nhttps://github.com/nothings/stb\n\n";

// stb_image.h embeds its license inline in a ~8000-line, mostly-code header
// with no separate LICENSE file to point a build-time fetch/embed step at
// (unlike Dear ImGui's LICENSE.txt or nlohmann/json's LICENSE.MIT, both
// handled by cmake/EmbedThirdPartyLicenses.cmake instead). Copied verbatim
// from the exact pinned commit installer/vendor.sh vendors
// (31c1ad37456438565541f4919958214b6e762fb4) -- re-verify this text against
// the new pinned commit's license block if that pin is ever bumped.
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
        writeEntry(file, kImguiHeading, kImguiLicenseTxt, kImguiLicenseTxtLen);
        writeEntry(file, kJsonHeading, kJsonLicenseTxt, kJsonLicenseTxtLen);
        writeEntry(file, kStbHeading, kStbLicenseTxt);
        file << kDivider;
    } catch (...) {
        // Best-effort -- a missing COPYRIGHT.txt must not fail an otherwise
        // successful install.
    }
}

} // namespace tuxblox
