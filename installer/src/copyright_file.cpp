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
#include "inter_ofl_license_txt.h"       // generated at build time
#include "json_license_txt.h"       // generated at build time
#include "lgpl21_license_txt.h"     // generated at build time
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

// Named only when the build was given the commit of the interface stack it ships, so a build without one names none of it
#ifdef TUXBLOX_LIBADWAITA_COMMIT
constexpr const char* kGtkHeading =
    "GTK 4 toolkit stack (used by TuxBlox's interface)\n"
    "https://www.gtk.org  |  https://gitlab.gnome.org/GNOME\n\n";

// The stack is installed as its own folder and the installer also carries a copy inside its own file, so the notice has to name each library and, for the ones whose license asks for it, say where the source is.
constexpr const char* kGtkLicenseTxt =
    "TuxBlox's interface is built on the libraries below. They are installed in\n"
    "the libtuxblox folder of the TuxBlox folder, and the installer carries its own copy\n"
    "inside its file, unpacked to ~/.cache/tuxblox/ on the first graphical run.\n"
    "They load as separate shared libraries. Each one is redistributed unmodified,\n"
    "except libadwaita, which has its own entry below the license text.\n"
    "\n"
    "  GTK 4.18.6             LGPL-2.1-or-later   https://download.gnome.org/sources/gtk/\n"
    "  GLib 2.84.4            LGPL-2.1-or-later   https://download.gnome.org/sources/glib/\n"
    "  Pango 1.56.4           LGPL-2.0-or-later   https://download.gnome.org/sources/pango/\n"
    "  gdk-pixbuf 2.42.12     LGPL-2.1-or-later   https://download.gnome.org/sources/gdk-pixbuf/\n"
    "  cairo 1.18.4           LGPL-2.1 or MPL-1.1 https://cairographics.org/releases/\n"
    "  FriBidi 1.0.16         LGPL-2.1-or-later   https://github.com/fribidi/fribidi\n"
    "  graphene 1.10.8        MIT                 https://github.com/ebassi/graphene\n"
    "  HarfBuzz 14.3.0        MIT                 https://github.com/harfbuzz/harfbuzz\n"
    "  FreeType 2.14.3        FreeType License    https://freetype.org\n"
    "  libepoxy 1.5.10        MIT                 https://github.com/anholt/libepoxy\n"
    "  libxkbcommon 1.6.0     MIT                 https://xkbcommon.org\n"
    "  Wayland 1.23.1         MIT                 https://wayland.freedesktop.org\n"
    "  pixman 0.44.2          MIT                 https://www.pixman.org\n"
    "  libffi 3.4.6           MIT                 https://github.com/libffi/libffi\n"
    "  libjpeg-turbo 3.1.0    BSD-3-Clause, IJG, Zlib\n"
    "                         https://libjpeg-turbo.org\n"
    "  libtiff 4.7.0          libtiff license (BSD-style)\n"
    "                         https://libtiff.gitlab.io/libtiff/\n"
    "  zstd 1.5.6             BSD-3-Clause        https://github.com/facebook/zstd\n"
    "\n"
    "The LGPL libraries above are free software: you can redistribute and modify\n"
    "them under the terms of the GNU Lesser General Public License, version 2.1\n"
    "or (for those that allow it) any later version, published by the Free\n"
    "Software Foundation. The full text of version 2.1 follows this entry. They are\n"
    "distributed in the hope that they will be useful, but WITHOUT ANY WARRANTY;\n"
    "without even the implied warranty of MERCHANTABILITY or FITNESS FOR A\n"
    "PARTICULAR PURPOSE. The MIT, BSD and FreeType licenses require that their\n"
    "copyright notices travel with the software; each project's own copyright\n"
    "notice is in its source at the address given.\n";

constexpr const char* kLgpl21Heading =
    "GNU Lesser General Public License, version 2.1 (GTK stack and libadwaita)\n"
    "https://www.gnu.org/licenses/old-licenses/lgpl-2.1.txt\n\n";

constexpr const char* kAdwaitaHeading =
    "libadwaita (MODIFIED; used by TuxBlox's interface)\n"
    "https://github.com/cherrypath0/libadwaita-compat\n\n";

// The fork is the one library in the stack TuxBlox changes, so its source address and exact commit are stated here for anyone who receives the binary.
constexpr const char* kAdwaitaLicenseTxt =
    "libadwaita is licensed under the GNU Lesser General Public License,\n"
    "version 2.1 or (at your option) any later version.\n"
    "\n"
    "The copy bundled in the TuxBlox Installer is MODIFIED. TuxBlox ships a fork of\n"
    "libadwaita 1.7.12 in which the dependency on AppStream has been removed, with\n"
    "the About dialogs changed to match. It is otherwise libadwaita as released by\n"
    "the GNOME project.\n"
    "\n"
    "The complete corresponding source code of the library you received, with\n"
    "TuxBlox's changes, is available at:\n"
    "\n"
    "  https://github.com/cherrypath0/libadwaita-compat\n"
    "  commit " TUXBLOX_LIBADWAITA_COMMIT "\n"
    "\n"
    "The upstream project is https://gitlab.gnome.org/GNOME/libadwaita. The\n"
    "library is loaded as a separate shared library from ~/.cache/tuxblox/, so\n"
    "you may replace it with a build of your own from that source, as the LGPL\n"
    "allows. The full text of the license is included in this file.\n";

constexpr const char* kAdwaitaSansHeading =
    "Adwaita Sans (font, used by TuxBlox's interface)\n"
    "https://gitlab.gnome.org/GNOME/adwaita-fonts\n\n";

constexpr const char* kAdwaitaSansLicenseTxt =
    "Adwaita Sans 48.2, from the GNOME project. It is derived from Inter, and is\n"
    "licensed under the SIL Open Font License, Version 1.1. Inter's own license\n"
    "text, which is the same license, is reproduced in full in the entry for Inter\n"
    "at the top of this file. The license is also at https://openfontlicense.org.\n";
#endif

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
        writeEntry(file, kJsonHeading, kJsonLicenseTxt, kJsonLicenseTxtLen);
        writeEntry(file, kStbHeading, kStbLicenseTxt);
        writeEntry(file, kSimpleIconsHeading, kSimpleIconsLicenseTxt);
#ifdef TUXBLOX_LIBADWAITA_COMMIT
        writeEntry(file, kGtkHeading, kGtkLicenseTxt);
        writeEntry(file, kLgpl21Heading, kLgpl21LicenseTxt, kLgpl21LicenseTxtLen);
        writeEntry(file, kAdwaitaHeading, kAdwaitaLicenseTxt);
        writeEntry(file, kAdwaitaSansHeading, kAdwaitaSansLicenseTxt);
#endif
        file << kDivider;
    } catch (...) {
        // Best-effort -- a missing COPYRIGHT.txt must not fail an otherwise
        // successful launch.
    }
}

} // namespace tuxblox
