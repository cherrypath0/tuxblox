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

// Byte-identical to installer/src/copyright_file.cpp -- keep the two in sync.
//
// Both the launcher and the installer write this same ~/.tuxblox/COPYRIGHT.txt
// (the launcher on every startup, the installer at the end of every install),
// so whichever ran last wins. They therefore have to emit the same thing, and
// that thing has to cover the whole installed product rather than only the
// components of the binary doing the writing -- otherwise the file's contents
// flip back and forth depending on what ran last.

#include "copyright_file.h"
#include "inter_ofl_license_txt.h"  // generated at build time
#include "imgui_license_txt.h"      // generated at build time
#include "json_license_txt.h"       // generated at build time
#include "lgpl3_license_txt.h"      // generated at build time
#include <fstream>

namespace tuxblox {

namespace {

constexpr const char* kCopyrightIntro =
    "This file contains the complete copyright notices and license texts for\n"
    "all third-party software components, fonts, and dependencies used within\n"
    "this application -- both the TuxBlox Launcher and the TuxBlox Installer.\n"
    "\n"
    "These components are provided under their respective open-source licenses,\n"
    "as detailed below.\n"
    "\n"
    "TuxBlox itself is licensed under the GNU General Public License v3; that\n"
    "license text is in the LICENSE file alongside this one.\n"
    "\n";

constexpr const char* kDivider =
    "================================================================================\n";

constexpr const char* kInterHeading = "Inter (font)\nhttps://github.com/rsms/inter\n\n";
constexpr const char* kImguiHeading =
    "Dear ImGui (used by the TuxBlox Installer's UI)\nhttps://github.com/ocornut/imgui\n\n";
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

constexpr const char* kQtHeading =
    "Qt 6.6.3 (used by the TuxBlox Launcher's UI)\n"
    "https://www.qt.io\n\n";

// LGPLv3 is the one bundled component whose license imposes obligations
// beyond attribution, so its entry says which libraries are covered, that
// they're unmodified, and how to obtain and substitute the source.
constexpr const char* kQtPreamble =
    "The TuxBlox Launcher links dynamically against the Qt 6 libraries listed\n"
    "below, which are redistributed with it, unmodified, under the terms of the\n"
    "GNU Lesser General Public License version 3 (LGPLv3).\n"
    "\n"
    "Bundled Qt libraries (in libtuxblox/lib/, with the platform plugin in\n"
    "libtuxblox/plugins/platforms/):\n"
    "  libQt6Core.so.6.6.3      libQt6Gui.so.6.6.3\n"
    "  libQt6Widgets.so.6.6.3   libQt6DBus.so.6.6.3\n"
    "  libQt6OpenGL.so.6.6.3    libQt6XcbQpa.so.6.6.3\n"
    "  plugins/platforms/libqxcb.so\n"
    "\n"
    "These are the official prebuilt open-source Qt 6.6.3 (gcc_64) binaries\n"
    "published by The Qt Company, installed at build time via aqtinstall. They\n"
    "are redistributed as-is, with no modifications to Qt itself.\n"
    "\n"
    "Corresponding source code for these libraries is available from\n"
    "https://download.qt.io/archive/qt/6.6/6.6.3/single/ and from the official\n"
    "Qt repositories at https://code.qt.io.\n"
    "\n"
    "As required by LGPLv3 section 4, you may modify Qt and relink the launcher\n"
    "against your own build: the Qt libraries are loaded dynamically from\n"
    "libtuxblox/lib/, so replacing the .so files in that directory with\n"
    "ABI-compatible builds of your own is sufficient -- no rebuild of TuxBlox is\n"
    "required.\n"
    "\n"
    "LGPLv3 incorporates the terms of the GNU General Public License version 3\n"
    "by reference; that GPLv3 text is in the LICENSE file alongside this one.\n"
    "\n"
    "The full text of the GNU Lesser General Public License version 3 follows.\n"
    "\n";

constexpr const char* kIcuHeading =
    "ICU 56 (International Components for Unicode, bundled with Qt)\n"
    "https://icu.unicode.org\n\n";

// ICU 56's license ships only as icu4c/license.html at the release-56-1 tag
// (there's no plain-text LICENSE to fetch at that tag), so the text is
// hardcoded here rather than downloaded -- same approach as stb_image.h above.
constexpr const char* kIcuLicenseTxt =
    "ICU License - ICU 1.8.1 and later\n"
    "\n"
    "COPYRIGHT AND PERMISSION NOTICE\n"
    "\n"
    "Copyright (c) 1995-2015 International Business Machines Corporation and others\n"
    "\n"
    "All rights reserved.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
    "of this software and associated documentation files (the \"Software\"),\n"
    "to deal in the Software without restriction, including without limitation\n"
    "the rights to use, copy, modify, merge, publish, distribute, and/or sell\n"
    "copies of the Software, and to permit persons\n"
    "to whom the Software is furnished to do so, provided that the above\n"
    "copyright notice(s) and this permission notice appear in all copies\n"
    "of the Software and that both the above copyright notice(s) and this\n"
    "permission notice appear in supporting documentation.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
    "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
    "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS.\n"
    "IN NO EVENT SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE BE\n"
    "LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY\n"
    "DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN\n"
    "ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN\n"
    "CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.\n"
    "\n"
    "Except as contained in this notice, the name of a copyright holder shall not be\n"
    "used in advertising or otherwise to promote the sale, use or other dealings in\n"
    "this Software without prior written authorization of the copyright holder.\n"
    "\n"
    "All trademarks and registered trademarks mentioned herein are the property of\n"
    "their respective owners.\n";

constexpr const char* kXcbHeading =
    "X.Org / xkbcommon client libraries (bundled with the Qt xcb platform plugin)\n"
    "https://gitlab.freedesktop.org/xorg/lib  |  https://xkbcommon.org\n\n";

// One entry for the whole group: these are the Ubuntu 20.04 system libraries
// bundle-qt.sh copies in beside Qt (see its SYSTEM_LIB_ROOTS list). They are
// all MIT/X11-style; libxcb's notice is reproduced in full as the
// representative text, with upstreams listed for the per-file breakdown.
constexpr const char* kXcbLicenseTxt =
    "Bundled in libtuxblox/lib/:\n"
    "  libxcb-cursor.so.0        libxcb-icccm.so.4      libxcb-image.so.0\n"
    "  libxcb-keysyms.so.1       libxcb-randr.so.0      libxcb-render.so.0\n"
    "  libxcb-render-util.so.0   libxcb-shape.so.0      libxcb-shm.so.0\n"
    "  libxcb-util.so.1          libxcb-xkb.so.1        libxkbcommon.so.0\n"
    "  libxkbcommon-x11.so.0\n"
    "\n"
    "These libraries are all distributed under MIT/X11-style permissive licenses.\n"
    "libxcb's notice is reproduced below as the representative text. For the\n"
    "complete per-file copyright and license breakdown -- xkbcommon in particular\n"
    "carries several MIT/X11 variants -- see the COPYING file in each project:\n"
    "https://gitlab.freedesktop.org/xorg/lib/libxcb/-/blob/master/COPYING and\n"
    "https://github.com/xkbcommon/libxkbcommon/blob/master/LICENSE\n"
    "\n"
    "Copyright (C) 2001-2006 Bart Massey, Jamey Sharp, and Josh Triplett.\n"
    "All Rights Reserved.\n"
    "\n"
    "Permission is hereby granted, free of charge, to any person\n"
    "obtaining a copy of this software and associated\n"
    "documentation files (the \"Software\"), to deal in the\n"
    "Software without restriction, including without limitation\n"
    "the rights to use, copy, modify, merge, publish, distribute,\n"
    "sublicense, and/or sell copies of the Software, and to\n"
    "permit persons to whom the Software is furnished to do so,\n"
    "subject to the following conditions:\n"
    "\n"
    "The above copyright notice and this permission notice shall\n"
    "be included in all copies or substantial portions of the\n"
    "Software.\n"
    "\n"
    "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY\n"
    "KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE\n"
    "WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR\n"
    "PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS\n"
    "BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER\n"
    "IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
    "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR\n"
    "OTHER DEALINGS IN THE SOFTWARE.\n"
    "\n"
    "Except as contained in this notice, the names of the authors\n"
    "or their institutions shall not be used in advertising or\n"
    "otherwise to promote the sale, use or other dealings in this\n"
    "Software without prior written authorization from the\n"
    "authors.\n";

void writeEntry(std::ofstream& file, const char* heading,
                 const unsigned char* text, std::size_t textLen) {
    file << kDivider << "\n" << heading;
    file.write(reinterpret_cast<const char*>(text), static_cast<std::streamsize>(textLen));
    file << "\n";
}

void writeEntry(std::ofstream& file, const char* heading, const char* text) {
    file << kDivider << "\n" << heading << text << "\n";
}

// Heading, then explanatory text of our own, then the embedded license.
void writeEntry(std::ofstream& file, const char* heading, const char* preamble,
                 const unsigned char* text, std::size_t textLen) {
    file << kDivider << "\n" << heading << preamble;
    file.write(reinterpret_cast<const char*>(text), static_cast<std::streamsize>(textLen));
    file << "\n";
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
        writeEntry(file, kSimpleIconsHeading, kSimpleIconsLicenseTxt);
        writeEntry(file, kQtHeading, kQtPreamble, kLgpl3LicenseTxt, kLgpl3LicenseTxtLen);
        writeEntry(file, kIcuHeading, kIcuLicenseTxt);
        writeEntry(file, kXcbHeading, kXcbLicenseTxt);
        file << kDivider;
    } catch (...) {
        // Best-effort -- a missing COPYRIGHT.txt must not fail an otherwise
        // successful launch.
    }
}

} // namespace tuxblox
