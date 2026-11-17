# WebKitGTK bundle - third-party license audit trail

This directory holds the verbatim upstream license text for every dependency the
`compat/webkitgtk/bundle/` build compiles from source and ships in the packaged tarball
(`webkitgtk-<version>-x86_64.tar.xz`), at the exact versions pinned in
`compat/webkitgtk/bundle/versions.env`. Each subdirectory is named after the dependency
(lowercase, matching `versions.env` reasonably) and contains the license file(s)
using upstream's own filename(s) (`LICENSE`, `COPYING`, `COPYING.LESSERv2`, etc.) —
nothing here is paraphrased, summarized, or reconstructed from memory; every file
was fetched live from the project's own repository at (or as close as possible to)
the pinned version tag, or extracted directly from the pinned-version release
tarball.

The entries added on 2026-11-05 were all taken the second way, out of the very archives
`compat/webkitgtk/bundle/sources/` holds and `sha256sums` pins, so each one is the licence
text that shipped with the exact bytes this bundle compiles rather than whatever a project
page serves today. They close a gap that had been growing: 3.7.1 vendored around 35
libraries that had previously been borrowed from the host, and this file was not extended
with them at the time, so it claimed a completeness it no longer had.

Two packages this bundle compiles have no entry here on purpose, because nothing from them
reaches the tarball: `xorgproto` and `xcb-proto` install headers and protocol descriptions
only. `wayland-protocols` is the same and is now excluded at packaging time as well.

## Dependency table

| Directory | versions.env var | Pinned version | SPDX license(s) | Files | Source |
|---|---|---|---|---|---|
| `webkitgtk/` | `WEBKITGTK_VERSION` | 2.52.5 | LGPL-2.0-or-later (JavaScriptCore), LGPL-2.0/2.1 (WebCore), BSD-2-Clause (Apple/WebKit/bmalloc/WebDriver code) — **no single top-level license**, see note | `Source/JavaScriptCore/COPYING.LIB`, `Source/WebCore/LICENSE-LGPL-2`, `Source/WebCore/LICENSE-LGPL-2.1`, `Source/WebCore/LICENSE-APPLE` | `https://webkitgtk.org/releases/webkitgtk-2.52.5.tar.xz` (official release tarball, inspected directly) |
| `glib/` | `GLIB_VERSION` | 2.84.4 | LGPL-2.1-or-later | `COPYING` (pointer, resolved), `LICENSES/LGPL-2.1-or-later.txt` | `https://gitlab.gnome.org/GNOME/glib/-/raw/2.84.4/COPYING` and `.../LICENSES/LGPL-2.1-or-later.txt` |
| `cairo/` | `CAIRO_VERSION` | 1.18.4 | LGPL-2.1-only OR MPL-1.1 (recipient's choice) | `COPYING`, `COPYING-LGPL-2.1`, `COPYING-MPL-1.1` | `https://gitlab.freedesktop.org/cairo/cairo/-/raw/1.18.4/{COPYING,COPYING-LGPL-2.1,COPYING-MPL-1.1}` |
| `pango/` | `PANGO_VERSION` | 1.56.4 | LGPL-2.0-or-later | `COPYING` | `https://gitlab.gnome.org/GNOME/pango/-/raw/1.56.4/COPYING` |
| `gdk-pixbuf/` | `GDK_PIXBUF_VERSION` | 2.42.12 | LGPL-2.1-or-later | `COPYING` | `https://gitlab.gnome.org/GNOME/gdk-pixbuf/-/raw/2.42.12/COPYING` |
| `gtk4/` | `GTK4_VERSION` | 4.18.6 | LGPL-2.0-or-later | `COPYING` | `https://gitlab.gnome.org/GNOME/gtk/-/raw/4.18.6/COPYING` |
| `libsoup/` | `LIBSOUP_VERSION` | 3.6.5 | LGPL-2.0-or-later | `COPYING` | `https://gitlab.gnome.org/GNOME/libsoup/-/raw/3.6.5/COPYING` (confirmed against libsoup's own `meson.build` `license:` field) |
| `icu/` | `ICU_VERSION` | 77-1 (icu4c release-77-1 / 77.1) | Unicode-3.0 ("Unicode License v3"), plus a couple of small bundled third-party notices | `LICENSE` | `https://raw.githubusercontent.com/unicode-org/icu/release-77-1/LICENSE` |
| `libtasn1/` | `LIBTASN1_VERSION` | 4.20.0 | LGPL-2.1-or-later (library), GPL-3.0-or-later (CLI tools) | `COPYING`, `COPYING.LESSERv2` | `https://gitlab.com/gnutls/libtasn1/-/raw/v4.20.0/{COPYING,COPYING.LESSERv2}` |
| `sqlite/` | `SQLITE_VERSION` / `SQLITE_YEAR` | 3530400 / 2026 (~3.53.4) | Public domain (no SPDX identifier applies) | `LICENSE` (extracted from the copyright page; SQLite ships no LICENSE/COPYING file in-tree) | `https://www.sqlite.org/copyright.html` |
| `lcms2/` | `LCMS2_VERSION` | 2.17 | MIT | `LICENSE` | `https://raw.githubusercontent.com/mm2/Little-CMS/lcms2.17/LICENSE` |
| `libwebp/` | `LIBWEBP_VERSION` | 1.6.0 | BSD-3-Clause + separate patent grant | `COPYING`, `PATENTS` | `https://raw.githubusercontent.com/webmproject/libwebp/v1.6.0/{COPYING,PATENTS}` |
| `openjpeg/` | `OPENJPEG_VERSION` | 2.5.3 | BSD-2-Clause | `LICENSE` | `https://raw.githubusercontent.com/uclouvain/openjpeg/v2.5.3/LICENSE` |
| `libgudev/` | `LIBGUDEV_VERSION` | 238 | LGPL-2.1-or-later | `COPYING` | `https://gitlab.gnome.org/GNOME/libgudev/-/raw/238/COPYING` |
| `libsecret/` | `LIBSECRET_VERSION` | 0.21.7 | LGPL-2.1-or-later (library), Apache-2.0 (bundled test code only) | `COPYING`, `COPYING.TESTS` | `https://gitlab.gnome.org/GNOME/libsecret/-/raw/0.21.7/{COPYING,COPYING.TESTS}` |
| `mesa/` | `MESA_VERSION` | 25.1.8 (tag `mesa-25.1.8`) | MIT (primary), plus Apache-2.0, BSL-1.0, GPL-1.0-or-later, GPL-2.0-only, SGI-B-2.0 for specific bundled components | `license.rst`, `licenses/{Apache-2.0,BSL-1.0,GPL-1.0-or-later,GPL-2.0-only,MIT,SGI-B-2.0}` | `https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-25.1.8/docs/license.rst` and `.../licenses/<name>` |
| `gstreamer/` | `GSTREAMER_VERSION` | 1.26.5 | LGPL-2.1-or-later | `COPYING` | `https://gitlab.freedesktop.org/gstreamer/gstreamer/-/raw/1.26.5/subprojects/gstreamer/COPYING` |
| `gst-plugins-base/` | `GSTREAMER_VERSION` | 1.26.5 | LGPL-2.1-or-later | `COPYING` | `https://gitlab.freedesktop.org/gstreamer/gstreamer/-/raw/1.26.5/subprojects/gst-plugins-base/COPYING` |
| `gst-plugins-bad/` | `GSTREAMER_VERSION` | 1.26.5 | LGPL-2.1-or-later (see note — no GPL-only element is actually compiled into this bundle's build config) | `COPYING` | `https://gitlab.freedesktop.org/gstreamer/gstreamer/-/raw/1.26.5/subprojects/gst-plugins-bad/COPYING` |
| `harfbuzz/` | `HARFBUZZ_VERSION` | 14.3.0 | MIT (HarfBuzz's own customized "Old MIT" text, not stock boilerplate) | `COPYING` | `https://raw.githubusercontent.com/harfbuzz/harfbuzz/14.3.0/COPYING` |
| `graphene/` | `GRAPHENE_VERSION` | 1.10.8 | MIT | `LICENSE.txt` | `https://raw.githubusercontent.com/ebassi/graphene/1.10.8/LICENSE.txt` |
| `libpsl/` | `LIBPSL_VERSION` | 0.21.5 | MIT | `LICENSE` | `https://raw.githubusercontent.com/rockdaboot/libpsl/0.21.5/LICENSE` |
| `gmp/` | `GMP_VERSION` | 6.3.0 | LGPL-3.0-or-later OR GPL-2.0-or-later (dual, user's choice) | `COPYING.LESSERv3`, `COPYINGv2`, `COPYINGv3` | `https://gmplib.org/download/gmp/gmp-6.3.0.tar.xz` (official tarball, license files extracted directly) |
| `nettle/` | `NETTLE_VERSION` | 3.10.2 | LGPL-3.0-or-later OR GPL-2.0-or-later (dual, user's choice) | `COPYING.LESSERv3`, `COPYINGv2`, `COPYINGv3` | `https://ftp.gnu.org/gnu/nettle/nettle-3.10.2.tar.gz` (official tarball, license files extracted directly) |
| `p11-kit/` | `P11KIT_VERSION` | 0.26.5 | BSD-3-Clause | `COPYING` | `https://raw.githubusercontent.com/p11-glue/p11-kit/0.26.5/COPYING` |
| `gnutls/` | `GNUTLS_VERSION` | 3.8.13 | LGPL-2.1-or-later (library), GPL-3.0-or-later (CLI utilities) | `COPYING` (GPLv3), `COPYING.LESSERv2` (LGPLv2.1) | `https://gitlab.com/gnutls/gnutls/-/raw/3.8.13/{COPYING,COPYING.LESSERv2}` |
| `glib-networking/` | `GLIB_NETWORKING_VERSION` | 2.80.1 | LGPL-2.1-or-later (with an OpenSSL linking exception) | `COPYING`, `LICENSE_EXCEPTION` | `https://gitlab.gnome.org/GNOME/glib-networking/-/raw/2.80.1/{COPYING,LICENSE_EXCEPTION}` |
| `libjpeg-turbo/` | `LIBJPEG_TURBO_VERSION` | 3.2.0 | BSD-3-Clause (own code) + IJG (inherited libjpeg code) + Zlib (SIMD code, referenced only — no standalone file at this tag; see note) | `LICENSE.md`, `README.ijg` | `https://raw.githubusercontent.com/libjpeg-turbo/libjpeg-turbo/3.2.0/{LICENSE.md,README.ijg}` |
| `libxml2/` | `LIBXML2_VERSION` | 2.15.3 (tag `v2.15.3`) | MIT | `Copyright` | `https://gitlab.gnome.org/GNOME/libxml2/-/raw/v2.15.3/Copyright` |
| `libxslt/` | `LIBXSLT_VERSION` | 1.1.45 (tag `v1.1.45`) | MIT-style (libxslt's own text; combined document covering libxslt + libexslt) | `Copyright` | `https://gitlab.gnome.org/GNOME/libxslt/-/raw/v1.1.45/Copyright` |
| `pcre2/` | *(not in versions.env — real transitive dependency, see note)* | 10.44 | BSD-3-Clause-style (PCRE2's own text) | `LICENCE` | `https://raw.githubusercontent.com/PCRE2Project/pcre2/pcre2-10.44/LICENCE` |
| `brotli/` | `BROTLI_VERSION` | 1.1.0 | MIT | `LICENSE` | `https://github.com/google/brotli/archive/refs/tags/v1.1.0.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `bzip2/` | `BZIP2_VERSION` | 1.0.8 | bzip2-1.0.6 | `LICENSE` | `https://sourceware.org/pub/bzip2/bzip2-1.0.8.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `expat/` | `EXPAT_VERSION` | 2.6.4 | MIT | `COPYING` | `https://github.com/libexpat/libexpat/releases/download/R_2_6_4/expat-2.6.4.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `fontconfig/` | `FONTCONFIG_VERSION` | 2.16.0 | MIT (Keith Packard's text, an HPND/MIT variant) | `COPYING` | `https://www.freedesktop.org/software/fontconfig/release/fontconfig-2.16.0.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `freetype/` | `FREETYPE_VERSION` | 2.14.3 | FTL OR GPL-2.0-or-later (dual, recipient's choice — the text says so explicitly) | `LICENSE.TXT` | `https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `fribidi/` | `FRIBIDI_VERSION` | 1.0.16 | LGPL-2.1-or-later | `COPYING` | `https://github.com/fribidi/fribidi/releases/download/v1.0.16/fribidi-1.0.16.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `gst-plugins-good/` | `GSTREAMER_VERSION` | 1.26.5 | LGPL-2.1-or-later | `COPYING` | `https://gstreamer.freedesktop.org/src/gst-plugins-good/gst-plugins-good-1.26.5.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libx11/` | `LIBX11_VERSION` | 1.8.10 | MIT/X11 plus BSD-style notices | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libX11-1.8.10.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxau/` | `LIBXAU_VERSION` | 1.0.11 | MIT (Open Group HPND-style) | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXau-1.0.11.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxcursor/` | `LIBXCURSOR_VERSION` | 1.2.2 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXcursor-1.2.2.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxdamage/` | `LIBXDAMAGE_VERSION` | 1.1.6 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXdamage-1.1.6.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxdmcp/` | `LIBXDMCP_VERSION` | 1.1.5 | MIT (Open Group HPND-style) | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXdmcp-1.1.5.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxext/` | `LIBXEXT_VERSION` | 1.3.6 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXext-1.3.6.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxfixes/` | `LIBXFIXES_VERSION` | 6.0.1 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXfixes-6.0.1.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxi/` | `LIBXI_VERSION` | 1.8.2 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXi-1.8.2.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxinerama/` | `LIBXINERAMA_VERSION` | 1.1.5 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXinerama-1.1.5.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxrandr/` | `LIBXRANDR_VERSION` | 1.5.4 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXrandr-1.5.4.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxrender/` | `LIBXRENDER_VERSION` | 0.9.11 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXrender-0.9.11.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxtst/` | `LIBXTST_VERSION` | 1.2.5 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXtst-1.2.5.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxv/` | `LIBXV_VERSION` | 1.0.12 | MIT/X11-style (DEC and MIT, with a name-use restriction) | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXv-1.0.12.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxxf86vm/` | `LIBXXF86VM_VERSION` | 1.1.5 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libXxf86vm-1.1.5.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libdatrie/` | `LIBDATRIE_VERSION` | 0.2.13 | LGPL-2.1-or-later | `COPYING` | `https://github.com/tlwg/libdatrie/releases/download/v0.2.13/libdatrie-0.2.13.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libdrm/` | `LIBDRM_VERSION` | 2.4.125 | MIT | `LICENSE` | `https://dri.freedesktop.org/libdrm/libdrm-2.4.125.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libepoxy/` | `LIBEPOXY_VERSION` | 1.5.10 | MIT | `COPYING` | `https://download.gnome.org/sources/libepoxy/1.5/libepoxy-1.5.10.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libffi/` | `LIBFFI_VERSION` | 3.4.6 | MIT (library); GPL-2.0-or-later with autoconf exception (`LICENSE-BUILDTOOLS`, build scripts only) | `LICENSE`, `LICENSE-BUILDTOOLS` | `https://github.com/libffi/libffi/releases/download/v3.4.6/libffi-3.4.6.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libgcrypt/` | `LIBGCRYPT_VERSION` | 1.11.0 | LGPL-2.1-or-later (library); GPL-2.0-or-later (tools); MIT and BSD notices for bundled code (`LICENSES`) | `COPYING`, `COPYING.LIB`, `LICENSES` | `https://gnupg.org/ftp/gcrypt/libgcrypt/libgcrypt-1.11.0.tar.bz2` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libglvnd/` | `LIBGLVND_VERSION` | 1.7.0 | MIT (NVIDIA's text); plus X.Org, Mesa, uthash, buildconf and ax-pthread notices for bundled components | `LICENSE`, `src/util/cJSON/LICENSE`, `src/util/uthash/LICENSE` | `https://gitlab.freedesktop.org/glvnd/libglvnd/-/archive/v1.7.0/libglvnd-v1.7.0.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libgpg-error/` | `LIBGPG_ERROR_VERSION` | 1.50 | LGPL-2.1-or-later (library); GPL-2.0-or-later (tools) | `COPYING`, `COPYING.LIB` | `https://gnupg.org/ftp/gcrypt/libgpg-error/libgpg-error-1.50.tar.bz2` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libidn2/` | `LIBIDN2_VERSION` | 2.3.7 | LGPL-3.0-or-later OR GPL-2.0-or-later (dual); Unicode terms for the bundled tables (`COPYING.unicode`) | `COPYING`, `COPYING.LESSERv3`, `COPYING.unicode`, `COPYINGv2` | `https://ftp.gnu.org/gnu/libidn/libidn2-2.3.7.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libpng/` | `LIBPNG_VERSION` | 1.6.44 | Libpng ("PNG Reference Library License version 2") | `LICENSE` | `https://download.sourceforge.net/libpng/libpng-1.6.44.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libthai/` | `LIBTHAI_VERSION` | 0.1.29 | LGPL-2.1-or-later | `COPYING` | `https://github.com/tlwg/libthai/releases/download/v0.1.29/libthai-0.1.29.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxcb/` | `LIBXCB_VERSION` | 1.17.0 | MIT | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libxcb-1.17.0.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxkbcommon/` | `XKBCOMMON_VERSION` | 1.7.0 | MIT | `LICENSE` | `https://xkbcommon.org/download/libxkbcommon-1.7.0.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libxshmfence/` | `LIBXSHMFENCE_VERSION` | 1.3.2 | MIT/X11-style | `COPYING` | `https://xorg.freedesktop.org/releases/individual/lib/libxshmfence-1.3.2.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `nghttp2/` | `LIBNGHTTP2_VERSION` | 1.64.0 | MIT | `COPYING` | `https://github.com/nghttp2/nghttp2/releases/download/v1.64.0/nghttp2-1.64.0.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `pixman/` | `PIXMAN_VERSION` | 0.44.2 | MIT | `COPYING` | `https://www.cairographics.org/releases/pixman-0.44.2.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `libtiff/` | `LIBTIFF_VERSION` | 4.7.0 | libtiff (HPND-style, Sam Leffler / SGI) | `LICENSE.md` | `https://download.osgeo.org/libtiff/tiff-4.7.0.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `wayland/` | `WAYLAND_VERSION` | 1.23.0 | MIT | `COPYING` | `https://gitlab.freedesktop.org/wayland/wayland/-/releases/1.23.0/downloads/wayland-1.23.0.tar.xz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `zlib/` | `ZLIB_VERSION` | 1.3.1 | Zlib | `LICENSE` | `https://zlib.net/fossils/zlib-1.3.1.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |
| `zstd/` | `ZSTD_VERSION` | 1.5.6 | BSD-3-Clause (`LICENSE`) OR GPL-2.0-only (`COPYING`), dual at the recipient's choice | `COPYING`, `LICENSE` | `https://github.com/facebook/zstd/releases/download/v1.5.6/zstd-1.5.6.tar.gz` (the pinned archive this build compiles, verified against `sha256sums`) |

## Notes on individual entries

- **libdrm**: ships no `COPYING`/`LICENSE` file of any kind. The MIT grant it is
  distributed under is stated in its own core header, `include/drm/drm.h`, and
  `LICENSE` here is that notice copied out of it verbatim with the C comment markers
  removed and nothing else changed. Same approach as the `sqlite` entry below, which
  also has no in-tree licence file.
- **libglvnd**: likewise ships no top-level licence file. `LICENSE` here is the licence
  section of its own `README.md`, verbatim apart from removing the four-space markdown
  indent -- it covers libglvnd's own MIT text plus the separate X.Org, Mesa, uthash,
  buildconf and ax-pthread notices for the code it bundles. The two bundled components
  that *do* carry their own files keep them at their upstream paths
  (`src/util/cJSON/LICENSE`, `src/util/uthash/LICENSE`).
- **libwpe / wpebackend-fdo**: removed on 2026-11-05 along with their build steps. They
  are the WPE port's windowing abstraction and this bundle builds the GTK port, which
  never linked them -- see `build-in-container.sh` where the build steps used to be.

- **webkitgtk**: WebKit has no single canonical top-level LICENSE covering the
  whole project — licensing is expressed per-file/per-directory. The four files
  captured here were chosen as representative of the real, verified license mix
  (confirmed against actual per-file SPDX/license headers sampled across
  JavaScriptCore, WebCore, WebKit/WebProcess, bmalloc, and WebDriver): LGPL for
  JavaScriptCore, LGPL-2/2.1 for WebCore, and a BSD-2-Clause "Apple" variant used
  verbatim across WebKit/bmalloc/WebDriver source headers. **Not fully captured**:
  WebKit vendors a large number of additional third-party libraries under
  `Source/ThirdParty/` (ANGLE, Skia, pdf.js, its own bundled ICU copy, various ISO
  C++ reference implementations, etc.), each with its own license, which were not
  individually inventoried here. Flagging this as a known gap — see "Unverified /
  needs follow-up" below.
- **pango** and **gtk4**: both genuinely ship the older "GNU Library General
  Public License Version 2" text (LGPL-2.0), not LGPL-2.1, despite that being the
  common assumption for GNOME-stack libraries — confirmed against actual bundled
  `COPYING` content and per-file source headers ("version 2 ... or later"),
  captured as LGPL-2.0-or-later accordingly.
- **libsoup**: same correction as above — the bundled `COPYING` is LGPL-2.0, not
  2.1; confirmed additionally via libsoup's own `meson.build` `license:` field.
- **libtasn1**: package genuinely splits license by directory (library = LGPL,
  CLI tools = GPL); upstream's own LGPL file is literally named
  `COPYING.LESSERv2`, not the more generic `COPYING.LESSER`.
- **gmp** / **nettle**: both are dual LGPLv3-or-later / GPLv2-or-later, confirmed
  directly from each project's own bundled `README` inside the pinned-version
  tarball (not assumed from reputation). Both ship a legacy `COPYING` file that
  duplicates `COPYINGv3` byte-for-byte — that redundant duplicate was not copied
  in, only the three distinctly-named files that together express the real
  license set.
- **gnutls**: package splits license by component (library = LGPL-2.1, CLI
  utilities = GPL-3.0); upstream's own LGPL file is named `COPYING.LESSERv2`.
- **gst-plugins-bad**: its own top-level `COPYING` is genuinely LGPL-2.1 (not
  GPL), confirmed by content. Individually checked GStreamer's own wrapper source
  files for elements that can link against historically-GPL external codec
  libraries (faad2, x265, libdvdnav/libdvdread, libdca, mjpegtools) — every
  GStreamer-authored wrapper file carries an LGPL header, and none of those GPL
  external libraries are installed in this bundle's build container
  (`compat/webkitgtk/bundle/Containerfile`) or enabled by `build-in-container.sh`'s bare
  `meson setup` invocation, so no GPL-only code is actually compiled into this
  bundle's shipped `gst-plugins-bad` output. LGPL-2.1-or-later is correct and
  sufficient for what's actually built and shipped here.
- **libjpeg-turbo**: genuinely a three-way split. `LICENSE.md` embeds the
  BSD-3-Clause text and explicitly points to a separate `README.ijg` file for the
  IJG License (which was fetched and included here, since `LICENSE.md` alone is
  incomplete without it — upstream's own redistribution terms require shipping
  `README.ijg` unmodified alongside `LICENSE.md`). The third leg (Zlib, covering
  the SIMD sources) is referenced by URL inside `LICENSE.md` rather than shipped
  as a standalone file — upstream itself does not bundle a separate zlib-license
  file at the repository root at this tag (the zlib terms live in individual SIMD
  source file headers instead), so this matches upstream's own structure rather
  than being an omission on our part.
- **pcre2**: **not listed in `compat/webkitgtk/bundle/versions.env`** — this is a real,
  previously-unaccounted-for transitive dependency. `compat/webkitgtk/bundle/Containerfile`
  deliberately does not install a system `libpcre2-dev` package, and
  `build-in-container.sh` never fetches PCRE2 explicitly, yet
  `compat/webkitgtk/bundle/package.sh`'s own comments confirm `libpcre2-{8,16,32}.a`
  static archives end up present in the built prefix. This happens because GLib
  2.84.4's own meson build declares a wrap-subproject fallback for PCRE2 (see
  `https://raw.githubusercontent.com/GNOME/glib/2.84.4/subprojects/pcre2.wrap`,
  re-fetched and confirmed during this audit), which meson auto-downloads and
  builds when no system copy is found — pinning PCRE2 to version **10.44**. PCRE2
  is therefore genuinely compiled and shipped by this bundle even though it isn't
  named in `versions.env`. Recommend `compat/webkitgtk/bundle/versions.env` be updated to
  track this explicitly (e.g. `PCRE2_VERSION=10.44`) so future GLib bumps don't
  silently drift this pin — flagging for maintainer follow-up, not fixed here per
  the instruction not to modify anything outside `third_party_licenses/webkitgtk/`.
- **mesa**: this bundle builds mesa in software-only mode (`softpipe`/`osmesa`
  gallium driver, no Vulkan drivers) per `compat/webkitgtk/bundle/build-in-container.sh`,
  which does not change which files make up Mesa's own authoritative license set
  — `docs/license.rst` and its referenced `licenses/` directory are Mesa's
  project-wide statement regardless of which drivers a given downstream build
  enables, so all six files were captured together rather than trying to narrow
  them to only the enabled driver's specific licenses.
- **sqlite**: SQLite ships no LICENSE/COPYING file in its own source distribution
  at all — `https://www.sqlite.org/copyright.html` is genuinely the authoritative,
  canonical source for its public-domain dedication (confirmed via SQLite's own
  documentation). This is a live page rather than a version-pinned artifact, which
  is unavoidable since the public-domain dedication itself is not version-specific
  and SQLite doesn't tag per-release license text separately from its source tags.
- **icu**: `ICU_VERSION=77-1` corresponds to icu4c `release-77-1` (ICU 77.1).
  Confirmed the applicable license is the unified "Unicode License v3"
  (`Unicode-3.0`), used by ICU 73 onward — the bundled `LICENSE` file also
  contains a couple of small incidental third-party notices (an autoconf-script
  GPLv3 exception, an MIT-style notice for `install-sh`), preserved verbatim as
  part of ICU's own single combined license file.

## Unverified / needs follow-up

- **webkitgtk**: the representative LGPL/BSD files captured do not cover the
  numerous additional third-party-vendored components under WebKit's own
  `Source/ThirdParty/` and parts of `Source/WTF/` (ANGLE, Skia, pdf.js, an
  internally-vendored ICU copy, various small ISO C++ reference libraries, etc.).
  These each carry their own (mostly permissive) licenses but were out of scope
  for this pass — a human should decide whether full sub-vendor license inventory
  is warranted for this bundle, given WebKit's own upstream tree does not ship a
  single consolidated NOTICE covering them either.
- **pcre2**: version pin (10.44) was derived indirectly, by inspecting GLib
  2.84.4's meson wrap file rather than from an explicit project pin — correct as
  of this audit date, but will silently drift out of sync if `GLIB_VERSION` is
  bumped in the future without re-checking. Recommend adding an explicit
  `PCRE2_VERSION` to `compat/webkitgtk/bundle/versions.env` (maintainer action, not made
  here — out of this task's scope, which is licenses only).
- **libjpeg-turbo**: the Zlib-licensed leg of its 3-way license split is captured
  only as a reference/pointer (matching upstream's own structure), not as a
  standalone extracted license file, since no such standalone file exists in the
  upstream repo at this tag.

All other entries above were fetched and verified directly against the pinned
version's own repository tag or official release tarball, with license content
cross-checked against actual per-file source headers or project metadata
(`meson.build` license fields, README license statements) where the correct SPDX
identifier was non-obvious.
