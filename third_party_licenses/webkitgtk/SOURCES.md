# WebKitGTK bundle - third-party license audit trail

This directory holds the verbatim upstream license text for every dependency the
`webkitgtk-bundle/` build compiles from source and ships in the packaged tarball
(`webkitgtk-<version>-x86_64.tar.xz`), at the exact versions pinned in
`webkitgtk-bundle/versions.env`. Each subdirectory is named after the dependency
(lowercase, matching `versions.env` reasonably) and contains the license file(s)
using upstream's own filename(s) (`LICENSE`, `COPYING`, `COPYING.LESSERv2`, etc.) —
nothing here is paraphrased, summarized, or reconstructed from memory; every file
was fetched live from the project's own repository at (or as close as possible to)
the pinned version tag, or extracted directly from the pinned-version release
tarball.

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
| `wpebackend-fdo/` | `WPEBACKEND_FDO_VERSION` | 1.14.2 | BSD-2-Clause | `COPYING` | `https://raw.githubusercontent.com/Igalia/WPEBackend-fdo/1.14.2/COPYING` (upstream org moved from `WebPlatformForEmbedded` to `Igalia`) |
| `libwpe/` | `LIBWPE_VERSION` | 1.16.3 | BSD-2-Clause | `COPYING` | `https://raw.githubusercontent.com/WebPlatformForEmbedded/libwpe/1.16.3/COPYING` |
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

## Notes on individual entries

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
  (`webkitgtk-bundle/Containerfile`) or enabled by `build-in-container.sh`'s bare
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
- **pcre2**: **not listed in `webkitgtk-bundle/versions.env`** — this is a real,
  previously-unaccounted-for transitive dependency. `webkitgtk-bundle/Containerfile`
  deliberately does not install a system `libpcre2-dev` package, and
  `build-in-container.sh` never fetches PCRE2 explicitly, yet
  `webkitgtk-bundle/package.sh`'s own comments confirm `libpcre2-{8,16,32}.a`
  static archives end up present in the built prefix. This happens because GLib
  2.84.4's own meson build declares a wrap-subproject fallback for PCRE2 (see
  `https://raw.githubusercontent.com/GNOME/glib/2.84.4/subprojects/pcre2.wrap`,
  re-fetched and confirmed during this audit), which meson auto-downloads and
  builds when no system copy is found — pinning PCRE2 to version **10.44**. PCRE2
  is therefore genuinely compiled and shipped by this bundle even though it isn't
  named in `versions.env`. Recommend `webkitgtk-bundle/versions.env` be updated to
  track this explicitly (e.g. `PCRE2_VERSION=10.44`) so future GLib bumps don't
  silently drift this pin — flagging for maintainer follow-up, not fixed here per
  the instruction not to modify anything outside `third_party_licenses/webkitgtk/`.
- **mesa**: this bundle builds mesa in software-only mode (`softpipe`/`osmesa`
  gallium driver, no Vulkan drivers) per `webkitgtk-bundle/build-in-container.sh`,
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
  `PCRE2_VERSION` to `webkitgtk-bundle/versions.env` (maintainer action, not made
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
