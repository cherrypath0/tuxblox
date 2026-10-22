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

#include "host_icons.h"

#include "svg_raster.h"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Third Parties
#include <gtk/gtk.h>

namespace tuxblox {

namespace {

const char *const ControlIcons[] = {
    "window-minimize-symbolic",
    "window-maximize-symbolic",
    "window-restore-symbolic",
    "window-close-symbolic",
};

// 16 is what a title bar asks for; 32 is there so a high-resolution screen has something to draw from rather than a stretched 16
const int RenderSizes[] = {16, 32};

const char ThemeName[] = "TuxBloxSystem";

// A desktop that points at one of these is pointing at TuxBlox's own icons or at no icons at all, so there is nothing of the desktop's to take
bool isOwnOrEmptyTheme(const std::string &name) {
    return name.empty() || name == ThemeName || name == "TuxBlox" || name == "hicolor";
}

std::string hostIconThemeName() {
    GtkSettings *pSettings = gtk_settings_get_default();
    if (pSettings == nullptr) return "";
    char *pName = nullptr;
    g_object_get(pSettings, "gtk-icon-theme-name", &pName, nullptr);
    const std::string name = pName != nullptr ? pName : "";
    g_free(pName);
    return name;
}

std::vector<std::string> iconSearchRoots() {
    std::vector<std::string> roots;
    roots.push_back(std::string(g_get_user_data_dir()) + "/icons");
    roots.push_back(std::string(g_get_home_dir()) + "/.icons");
    for (const char *const *ppDir = g_get_system_data_dirs(); *ppDir != nullptr; ++ppDir) {
        roots.push_back(std::string(*ppDir) + "/icons");
    }
    return roots;
}

// One file a theme offers for one icon name. `rank` is how far down the inherit chain its theme sat, so a closer theme always wins.
struct Candidate {
    std::string path;
    bool svg = false;
    int size = 0;
    int rank = 0;
    bool found = false;
};

// Better means: nearer the front of the inherit chain, then drawable at any size, then closest to the size a title bar asks for.
bool better(const Candidate &a, const Candidate &b) {
    if (!b.found) return true;
    if (a.rank != b.rank) return a.rank < b.rank;
    if (a.svg != b.svg) return a.svg;
    return std::abs(a.size - RenderSizes[0]) < std::abs(b.size - RenderSizes[0]);
}

// The folders of one theme that hold icons the right size, read once so the four lookups below do not re-parse the index
struct ThemeDir {
    std::string path;
    int size = 0;
    bool svg = false;
};

std::vector<ThemeDir> readThemeDirs(const std::string &themeRoot, std::vector<std::string> &inherits) {
    std::vector<ThemeDir> dirs;
    GKeyFile *pIndex = g_key_file_new();
    if (!g_key_file_load_from_file(pIndex, (themeRoot + "/index.theme").c_str(), G_KEY_FILE_NONE, nullptr)) {
        g_key_file_free(pIndex);
        return dirs;
    }
    // An icon theme separates its lists with commas, where a key file is read with semicolons unless it is told otherwise -- left alone, a theme's whole list of folders comes back as one unusable name.
    g_key_file_set_list_separator(pIndex, ',');

    gsize inheritCount = 0;
    char **ppInherits = g_key_file_get_string_list(pIndex, "Icon Theme", "Inherits", &inheritCount, nullptr);
    for (gsize i = 0; i < inheritCount; ++i) inherits.push_back(ppInherits[i]);
    g_strfreev(ppInherits);

    gsize dirCount = 0;
    char **ppDirs = g_key_file_get_string_list(pIndex, "Icon Theme", "Directories", &dirCount, nullptr);
    for (gsize i = 0; i < dirCount; ++i) {
        const char *pDir = ppDirs[i];
        char *pType = g_key_file_get_string(pIndex, pDir, "Type", nullptr);
        const bool scalable = pType != nullptr && std::strcmp(pType, "Scalable") == 0;
        g_free(pType);
        const int size = g_key_file_get_integer(pIndex, pDir, "Size", nullptr);
        // Only the sizes a title bar can use. A full theme lists hundreds of folders and every one left in here costs a disk lookup for each of the four icons.
        if (!scalable && size != RenderSizes[0] && size != RenderSizes[1]) continue;
        dirs.push_back({themeRoot + "/" + pDir, scalable ? RenderSizes[0] : size, scalable});
    }
    g_strfreev(ppDirs);
    g_key_file_free(pIndex);
    return dirs;
}

void collectFromTheme(const std::vector<std::string> &roots, const std::string &theme, int rank,
                      Candidate *pBest, std::vector<std::string> &inherits) {
    for (const std::string &root : roots) {
        const std::string themeRoot = root + "/" + theme;
        if (!g_file_test(themeRoot.c_str(), G_FILE_TEST_IS_DIR)) continue;

        for (const ThemeDir &dir : readThemeDirs(themeRoot, inherits)) {
            for (size_t i = 0; i < G_N_ELEMENTS(ControlIcons); ++i) {
                // .symbolic.png first: it is the form GTK itself ships a pre-rendered symbolic icon in, so where a theme has one it is the one meant to be drawn small.
                for (const char *pSuffix : {".symbolic.png", ".png", ".svg"}) {
                    const std::string path = dir.path + "/" + ControlIcons[i] + pSuffix;
                    if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) continue;
                    const Candidate found{path, std::strcmp(pSuffix, ".svg") == 0, dir.size, rank, true};
                    if (better(found, pBest[i])) pBest[i] = found;
                    break;
                }
            }
        }
    }
}

// Black, with the shape carried entirely by how opaque each dot is. That is what the icons TuxBlox ships already are, and it is what lets whatever shows the icon colour it to match the text beside it.
GdkPixbuf *toMask(GdkPixbuf *pSource) {
    GdkPixbuf *pMask = gdk_pixbuf_add_alpha(pSource, FALSE, 0, 0, 0);
    if (pMask == nullptr) return nullptr;
    guchar *pPixels = gdk_pixbuf_get_pixels(pMask);
    const int stride = gdk_pixbuf_get_rowstride(pMask);
    const int width = gdk_pixbuf_get_width(pMask);
    const int height = gdk_pixbuf_get_height(pMask);
    for (int y = 0; y < height; ++y) {
        guchar *pRow = pPixels + y * stride;
        for (int x = 0; x < width; ++x) {
            pRow[x * 4 + 0] = 0;
            pRow[x * 4 + 1] = 0;
            pRow[x * 4 + 2] = 0;
        }
    }
    return pMask;
}

GdkPixbuf *renderSvg(const std::string &path, int size) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return nullptr;
    const std::string markup((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    std::vector<unsigned char> mask;
    if (!rasteriseSvgMask(markup.data(), markup.size(), size, mask)) return nullptr;

    // Handed to the picture, which frees it through the callback below
    auto *pPixels = static_cast<guchar *>(g_malloc(mask.size()));
    std::memcpy(pPixels, mask.data(), mask.size());
    return gdk_pixbuf_new_from_data(pPixels, GDK_COLORSPACE_RGB, TRUE, 8, size, size, size * 4,
                                    [](guchar *pTaken, gpointer) { g_free(pTaken); }, nullptr);
}

bool writeIcon(const Candidate &candidate, int size, const std::string &dest) {
    GdkPixbuf *pDrawn = nullptr;
    if (candidate.svg) {
        pDrawn = renderSvg(candidate.path, size);
    } else {
        GdkPixbuf *pLoaded = gdk_pixbuf_new_from_file_at_size(candidate.path.c_str(), size, size, nullptr);
        if (pLoaded != nullptr) {
            pDrawn = toMask(pLoaded);
            g_object_unref(pLoaded);
        }
    }
    if (pDrawn == nullptr) return false;

    const bool ok = gdk_pixbuf_savev(pDrawn, dest.c_str(), "png", nullptr, nullptr, nullptr);
    g_object_unref(pDrawn);
    return ok;
}

bool writeIndexFile(const std::string &themeRoot, const std::string &fallbackTheme) {
    std::string index = "[Icon Theme]\nName=";
    index += ThemeName;
    index += "\nComment=Window controls taken from the desktop\nInherits=";
    index += fallbackTheme;
    index += "\nDirectories=";
    for (size_t i = 0; i < G_N_ELEMENTS(RenderSizes); ++i) {
        if (i > 0) index += ",";
        index += std::to_string(RenderSizes[i]) + "x" + std::to_string(RenderSizes[i]) + "/actions";
    }
    index += "\n";
    for (int size : RenderSizes) {
        const std::string dir = std::to_string(size) + "x" + std::to_string(size) + "/actions";
        index += "\n[" + dir + "]\nSize=" + std::to_string(size) + "\nContext=Actions\nType=Fixed\n";
    }
    return g_file_set_contents((themeRoot + "/index.theme").c_str(), index.c_str(),
                               static_cast<gssize>(index.size()), nullptr);
}

std::string themeRootPath() {
    return hostWindowControlsDir() + "/" + ThemeName;
}

// Reading a desktop's whole icon index costs tens of milliseconds the first time, and it answers the same every run until the desktop is given a different set of icons, so the answer is kept and the name it was taken from is kept beside it.
// Icons edited inside a theme without renaming it are therefore not picked up until the desktop changes theme or the folder is deleted, which is the right way round: the slow path should be the rare one.
bool alreadyBuiltFor(const std::string &hostTheme) {
    const std::string themeRoot = themeRootPath();
    char *pRecorded = nullptr;
    if (!g_file_get_contents((themeRoot + "/source").c_str(), &pRecorded, nullptr, nullptr)) return false;
    const bool same = hostTheme == pRecorded;
    g_free(pRecorded);
    if (!same) return false;

    if (!g_file_test((themeRoot + "/index.theme").c_str(), G_FILE_TEST_IS_REGULAR)) return false;
    for (int size : RenderSizes) {
        const std::string dir = themeRoot + "/" + std::to_string(size) + "x" + std::to_string(size) + "/actions";
        for (const char *pIcon : ControlIcons) {
            const std::string path = dir + "/" + pIcon + ".symbolic.png";
            if (!g_file_test(path.c_str(), G_FILE_TEST_IS_REGULAR)) return false;
        }
    }
    return true;
}

} // namespace

std::string hostWindowControlsDir() {
    return std::string(g_get_user_cache_dir()) + "/tuxblox/icons";
}

std::string buildHostWindowControls(const std::string &fallbackTheme) {
    const std::string hostTheme = hostIconThemeName();
    if (isOwnOrEmptyTheme(hostTheme)) return "";
    if (alreadyBuiltFor(hostTheme)) return ThemeName;

    const std::vector<std::string> roots = iconSearchRoots();
    Candidate best[G_N_ELEMENTS(ControlIcons)];

    // Along the chain the desktop's theme declares, nearest first, exactly as the desktop itself reads it
    std::vector<std::string> queue = {hostTheme};
    std::vector<std::string> seen;
    for (size_t rank = 0; rank < queue.size() && rank < 16; ++rank) {
        const std::string theme = queue[rank];
        if (std::find(seen.begin(), seen.end(), theme) != seen.end()) continue;
        seen.push_back(theme);
        collectFromTheme(roots, theme, static_cast<int>(rank), best, queue);
    }

    for (const Candidate &candidate : best) {
        if (!candidate.found) return "";
    }

    const std::string themeRoot = themeRootPath();
    for (int size : RenderSizes) {
        const std::string dir = themeRoot + "/" + std::to_string(size) + "x" + std::to_string(size) + "/actions";
        if (g_mkdir_with_parents(dir.c_str(), 0755) != 0) return "";
        for (size_t i = 0; i < G_N_ELEMENTS(ControlIcons); ++i) {
            const std::string dest = dir + "/" + ControlIcons[i] + ".symbolic.png";
            if (!writeIcon(best[i], size, dest)) return "";
        }
    }
    if (!writeIndexFile(themeRoot, fallbackTheme)) return "";
    // Last, so a run that died partway through leaves no note claiming the folder is finished
    g_file_set_contents((themeRoot + "/source").c_str(), hostTheme.c_str(),
                        static_cast<gssize>(hostTheme.size()), nullptr);
    return ThemeName;
}

} // namespace tuxblox
