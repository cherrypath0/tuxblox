# webkitgtk-bundle

One-time (or per-version-bump) build tooling for the WebKitGTK tarball vendored at
`../ProtonSource/contrib/webkitgtk-<version>-x86_64.tar.xz`. This is release
engineering, not part of the normal `./build.sh` a user or contributor runs.

## When to run this

- WebKitGTK needs a security update (check https://webkitgtk.org/security/ periodically)
- A dependency in `versions.env` needs bumping for any other reason

## How to run it

    cd webkitgtk-bundle
    ./build.sh

Takes a long time (building the full GTK4/WebKitGTK stack from source) on a genuinely
clean run -- expect multiple hours, most of it a single-threaded (`JOBS=1`, see the
memory-history comment in `build-in-container.sh`) WebKitGTK compile. A re-run where
webkitgtk itself is unchanged (no patch/version bump) finishes in well under a minute:
its own source+build tree persists in the `webkitgtk-src-build` volume, so `ninja`
does a true incremental build and correctly does nothing. Every *other* dependency
(glib, gtk4, libsoup, icu, ...) still re-fetches and rebuilds from scratch on every
run regardless -- that's fine, since all of them combined take well under an hour, and
none of them get the same persistent-source treatment webkitgtk does. Produces
`out/webkitgtk-<version>-x86_64.tar.xz`. Copy it into `../ProtonSource/contrib/`,
update the `WEBKITGTK_VER`/tarball filename in `../ProtonSource/Makefile.in` to match,
and commit both together.

`build.sh` uses three persistent podman named volumes so a failed/interrupted run
doesn't force starting over from nothing:
- `webkitgtk-prefix` -- the actual `/opt/tuxblox-webview` install prefix. This is what
  `package.sh` packages. Safe to delete and let `build-in-container.sh` recreate it if
  you want a genuinely clean build.
- `webkitgtk-src-build` -- webkitgtk's own extracted source tree AND its cmake
  `_build` directory (mounted at `/build/webkitgtk`, the one dependency whose /build
  subdirectory isn't ephemeral -- see the comment at its `fetch_and_extract` call site
  in `build-in-container.sh`). This is what makes an unchanged re-run fast. Delete it
  to force a genuinely clean webkitgtk rebuild (e.g. if you suspect the persisted tree
  itself is corrupted) -- a version bump in `versions.env` does NOT require deleting
  this manually, the script detects the version change itself via a marker file and
  re-fetches automatically.
- `webkitgtk-ccache` -- compiler cache for the WebKitGTK step only, a second line of
  defense for cases the persisted `_build` tree doesn't cover on its own (e.g. after
  deleting `webkitgtk-src-build` but not this). Not part of the shippable artifact;
  safe to delete any time, just makes the next from-scratch build slower.

## Why a container instead of building on your own machine

Building directly on a maintainer's own machine bakes in whatever glibc that machine
happens to have as a floor no end user's older system can satisfy -- see project
memory `glibc_floor_installer_bug.md` for a real example of this exact class of bug.
The `Containerfile` here pins an old glibc baseline (Debian 12) specifically so the
resulting tarball works broadly, not just on the machine that built it.

## Runtime relocatability: RPATH is necessary but NOT sufficient

`package.sh` rewrites every ELF file's RPATH (shared libraries under `lib/` **and**
executables under `libexec/`, e.g. `WebKitWebProcess`) to a relative,
`$ORIGIN`-based path, so the extracted tarball's `.so`-to-`.so` and
executable-to-`.so` linking works regardless of where it ends up on disk. This is
real and necessary, but **on its own it is not enough** to make this bundle
relocatable. Several absolute `/opt/tuxblox-webview` paths get compiled in as
literal string constants during the build -- not RPATH/RUNPATH entries -- and
`patchelf` cannot touch those; a consumer of this tarball should set these
environment variables at runtime, computed relative to wherever the tarball actually
gets extracted (`$(DST_DIR)/lib/tuxblox-webview` in `ProtonSource/Makefile.in`'s
terms):

> **Read "LD_LIBRARY_PATH beats this bundle's RUNPATH" below before wiring this into
> Wine.** The RPATH work described here is correct in isolation, but Proton puts its
> own library directory ahead of it in the loader's search order inside every wine
> process -- which is exactly where Plans 2-4 will `dlopen()` this bundle.

| Variable | What it fixes | Value to set it to | Status |
|---|---|---|---|
| `WEBKIT_EXEC_PATH` | Where `libwebkitgtk-6.0.so` looks for `WebKitWebProcess`/`WebKitNetworkProcess`/`WebKitGPUProcess` | `<extract-dir>/libexec/webkitgtk-6.0` | **Required.** Verified end-to-end (real page load fails without it, succeeds with it). |
| `WEBKIT_INJECTED_BUNDLE_PATH` | Where `libwebkitgtk-6.0.so` looks for the web-process injected bundle (`libwebkitgtkinjectedbundle.so`) | `<extract-dir>/lib/webkitgtk-6.0/injected-bundle` | **Required** to avoid a load-time warning (page loads either way, but the injected bundle -- used for WebKit's internal JS-side extension hooks -- silently fails to load without it). Not in the original brief; found during WEBKIT_EXEC_PATH verification. |
| `GIO_EXTRA_MODULES` | GIO's TLS backend module (glib-networking's GnuTLS module) | `<extract-dir>/lib/x86_64-linux-gnu/gio/modules` | **Required.** Verified: `GTlsBackendDummy` (no TLS at all) without it, `GTlsBackendGnutls` (real) with it. |
| `GBM_BACKENDS_PATH` | Mesa's GBM backend search path (`libgbm`) | `<extract-dir>/lib/x86_64-linux-gnu/gbm` | **Required.** Verified against a real DRM render node: `gbm_create_device()` fails (logs the exact compiled-in `/opt/tuxblox-webview/...` path it tried) without it, succeeds with it. |
| `LIBGL_DRIVERS_PATH` | Mesa's DRI driver search path (`libGL`/`libEGL`) | `<extract-dir>/lib/x86_64-linux-gnu/dri` | **Not load-bearing for this specific build** (see below) -- set anyway, it's free insurance. |
| `GST_PLUGIN_SCANNER` | Where `libgstreamer-1.0.so` looks for the `gst-plugin-scanner` helper it spawns to introspect plugins out-of-process | `<extract-dir>/libexec/gstreamer-1.0/gst-plugin-scanner` | **Recommended, not independently verified to the same standard as the five above.** `libgstreamer-1.0.so` has a compiled-in absolute default (`/opt/tuxblox-webview/libexec/gstreamer-1.0/gst-plugin-scanner`) and its own `strings` output documents `GST_PLUGIN_SCANNER` as a real runtime override ("Trying GST_PLUGIN_SCANNER env var: %s") -- confirmed the mechanism exists, not exercised end-to-end with a real relocated GStreamer plugin load the way WEBKIT_EXEC_PATH was. |
| `GST_PLUGIN_SYSTEM_PATH` | Where GStreamer looks for its own plugin `.so`s (`libgst*.so` under `lib/x86_64-linux-gnu/gstreamer-1.0/`) | `<extract-dir>/lib/x86_64-linux-gnu/gstreamer-1.0` | Same confidence tier as `GST_PLUGIN_SCANNER` above -- a real, standard GStreamer env var (confirmed present in `libgstreamer-1.0.so`'s own strings output), not independently exercised end-to-end here. |
| `XDG_DATA_DIRS` | The general, standard freedesktop.org mechanism GLib/GTK use to find shared data (icon themes, mime info, and -- see below -- GSettings schemas) relative to an install prefix | `<extract-dir>/share` | Recommended as the general-purpose fallback; confirmed as a real, standard mechanism (present in `libgio-2.0.so`'s own strings output) but not independently exercised end-to-end here. |
| `GSETTINGS_SCHEMA_DIR` | The more specific GIO override for where compiled GSettings schemas (`share/glib-2.0/schemas/gschemas.compiled`) are found, taking precedence over `XDG_DATA_DIRS`-based discovery | `<extract-dir>/share/glib-2.0/schemas` | Same confidence tier as `XDG_DATA_DIRS` above -- confirmed real and present (`gschemas.compiled` genuinely ships in the bundle, pre-compiled at build time), not independently exercised end-to-end here. |

where `<extract-dir>` is the tarball's own root (i.e. what's inside the `webkitgtk/`
top-level directory the tarball's `--transform` wraps everything in -- for
`ProtonSource/Makefile.in`'s extraction rule, that's `$(DST_DIR)/lib/tuxblox-webview`
after `--strip-components=1`).

**On top of the environment variables above**, one file needs actual regeneration,
not just an env var: gdk-pixbuf's `loaders.cache` (`lib/x86_64-linux-gnu/gdk-pixbuf-2.0/
2.10.0/loaders.cache`) bakes each image-loader `.so`'s absolute path into the cache
FILE ITSELF at the time it was generated (this build's own `/opt/tuxblox-webview`) --
not an ELF dynamic-section entry `patchelf` can rewrite, and not a `getenv()` call any
environment variable can override. `ProtonSource/Makefile.in`'s extraction rule now
re-runs `gdk-pixbuf-query-loaders` (shipped in the tarball specifically for this,
under `libexec/gdk-pixbuf-tools/` -- `bin/` itself is still excluded) against the
just-extracted loaders directory immediately after extraction, once the real final
path is known, and overwrites `loaders.cache` with freshly-correct paths. If this
bundle's extraction location ever needs to move without going through
`ProtonSource/Makefile.in` (e.g. a manual re-extraction for testing), re-run that same
command by hand: `<extract-dir>/libexec/gdk-pixbuf-tools/gdk-pixbuf-query-loaders
<extract-dir>/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders/*.so >
<extract-dir>/lib/x86_64-linux-gnu/gdk-pixbuf-2.0/2.10.0/loaders.cache`.

### Why WEBKIT_EXEC_PATH needed an actual source patch, not just documentation

Upstream WebKitGTK's `WEBKIT_EXEC_PATH` environment-variable override
(`Source/WebKit/Shared/glib/ProcessExecutablePathGLib.cpp`) is gated behind
`#if ENABLE(DEVELOPER_MODE)`. This build does not (and should not) set
`-DDEVELOPER_MODE=ON` -- that flag has a much broader blast radius than this one
path-resolution fix needs (it also flips `DEVELOPER_MODE_FATAL_WARNINGS` on by
default, changes `ENABLE_THUNDER`/`USE_CAPSTONE` defaults, etc.). Verified directly
against the as-built (pre-fix) library:

```
$ strings lib/libwebkitgtk-6.0.so.4.16.9 | grep WEBKIT_EXEC_PATH
(no output)
```

i.e. the `g_getenv("WEBKIT_EXEC_PATH")` call was compiled out entirely -- the
environment variable genuinely did nothing on the as-built artifact, regardless of
what value it was set to. `build-in-container.sh` now applies a narrow source patch
(see the "Task 8 finding" comment block right before the WebKitGTK cmake invocation)
that moves *only* the `WEBKIT_EXEC_PATH` check outside the `DEVELOPER_MODE` guard,
leaving every other `DEVELOPER_MODE`-gated behavior in that function untouched.
Re-verified after the patched rebuild:

```
$ strings lib/libwebkitgtk-6.0.so.4.16.9 | grep -c '^WEBKIT_EXEC_PATH$'
1
```

`WEBKIT_INJECTED_BUNDLE_PATH` (`Source/WebKit/UIProcess/API/glib/WebKitWebContext.cpp`,
`injectedBundleDirectory()`) needed **no patch** -- upstream checks it unconditionally,
not gated behind `DEVELOPER_MODE` at all. Found by reading the same file family while
tracking down the `WEBKIT_EXEC_PATH` fix, after noticing a real
`Error loading the injected bundle (/opt/tuxblox-webview/lib/webkitgtk-6.0/...)`
warning during end-to-end testing (see below) that `WEBKIT_EXEC_PATH` alone didn't fix.

### Why GBM_BACKENDS_PATH / GIO_EXTRA_MODULES don't need a patch

Both are unconditional, always-compiled-in environment-variable overrides in
upstream Mesa (`src/gbm/main/backend.c`) and GLib/GIO respectively -- not gated
behind any equivalent of `DEVELOPER_MODE`. No source patch was needed; only
verification that the mechanism is real for this specific build (see "Verification
performed" below).

### LIBGL_DRIVERS_PATH: tested, found NOT load-bearing for this specific build

This is the one place this task's findings genuinely diverge from the brief's
assumption, discovered by testing rather than assumed. `strings` searched across
**every** file in the packaged tree (not just `libGL`/`libEGL`) for the literal
string `LIBGL_DRIVERS_PATH` and found **zero matches anywhere**:

```
$ find lib libexec -type f -name '*.so*' -o -type f ! -name '*.so*' | \
    xargs -I{} sh -c 'strings -a {} | grep -q LIBGL_DRIVERS_PATH && echo {}'
(no matches)
```

(Task 8 post-review correction, I-3: an earlier draft of this section used
`grep -rl 'LIBGL_DRIVERS_PATH' lib/ libexec/` here, which greps each file's raw
*binary* bytes directly -- unreliable for a multi-byte search string that can
straddle a `grep` internal buffer boundary or otherwise not line up the way it does
in decoded ASCII, and not actually how this finding was re-confirmed. `strings -a`
extracts printable-string runs first, the same tool and technique used throughout
the rest of this file's verification -- that's what actually gave the correct
"zero matches" answer above, and what should be trusted/reproduced going forward,
not a raw binary `grep -r`.)

An `eglInitialize()` test against `EGL_PLATFORM_SURFACELESS_MESA` (software
rendering, no real GPU driver needed) succeeds identically whether
`LIBGL_DRIVERS_PATH` is set or not, and `strace` confirms why: this Mesa build
(`-Dgallium-drivers=softpipe`, a single driver) never `dlopen()`s a separate
`swrast_dri.so`/`kms_swrast_dri.so` plugin file at all -- the software rasterizer is
linked directly into `libgallium-25.1.8.so`/`libEGL.so`, so the classic
`loader_open_driver_file()` code path in Mesa's `src/loader/loader.c` (the function
that *would* read `LIBGL_DRIVERS_PATH`) is simply never compiled into any library
this build produces. The `lib/x86_64-linux-gnu/dri/*.so` files still get built and
installed (they're unconditional `ninja install` targets) but appear to be unused by
this build's actual runtime dispatch. `GBM_BACKENDS_PATH` and `LIBGL_DRIVERS_PATH`
are NOT the same mechanism despite living in the same Mesa source tree -- GBM has its
own small, separate loader (which the string search and the functional test both
confirmed is real and load-bearing).

Practical takeaway: setting `LIBGL_DRIVERS_PATH` alongside the other four is harmless
and recommended defensively (e.g. a future Mesa rebuild that adds a real hardware
driver, or a different EGL platform WebKit's GPU process ends up using, might reach
the code path that *does* read it) -- but it should not be relied on as "the" fix for
any DRI-related relocation problem with the current build; if one turns up, root-cause
it fresh rather than assuming this variable is the missing piece.

### Verification performed

All findings above were tested for real, not assumed: the packaged tarball was
extracted to `/tmp/webkitgtk-verify` (a directory with **no relationship** to
`/opt/tuxblox-webview`, the build's own `$PREFIX`), and:

- **GIO_EXTRA_MODULES**: a small C program calling `g_tls_backend_get_default()`
  printed `GDummyTlsBackend` / `g_tls_backend_supports_tls(): FALSE` without the
  variable set, and `GTlsBackendGnutls` / `TRUE` with it set to the relocated
  `gio/modules` dir.
- **GBM_BACKENDS_PATH**: a small C program opening the host's real
  `/dev/dri/renderD128` (passed through via `podman run --device /dev/dri`) and
  calling `gbm_create_device()` failed with
  `MESA-LOADER: failed to open dri: /opt/tuxblox-webview/lib/x86_64-linux-gnu/gbm/dri_gbm.so: ... No such file or directory`
  (the exact compiled-in default, proving the mechanism AND showing precisely what's
  broken without the fix) when unset, and printed
  `PASS: gbm_create_device succeeded, backend name: drm` when set to the relocated
  `gbm` dir.
- **WEBKIT_EXEC_PATH** (+ **WEBKIT_INJECTED_BUNDLE_PATH**): the strongest test in this
  task -- a real GTK4 window + `WebKitWebView` loading `about:blank`, run against the
  host's actual X11 display (`--device /dev/dri`, `/tmp/.X11-unix` and `XAUTHORITY`
  bind-mounted into the container) and the relocated bundle only. Without
  `WEBKIT_EXEC_PATH` set, it failed with
  `Unable to spawn a new child process: Failed to spawn child process "/opt/tuxblox-webview/libexec/webkitgtk-6.0/WebKitNetworkProcess" (No such file or directory)`
  and aborted (`Trace/breakpoint trap`). With `WEBKIT_EXEC_PATH` (and
  `WEBKIT_INJECTED_BUNDLE_PATH`) set to the relocated paths, `load-changed` reached
  `WEBKIT_LOAD_FINISHED` -- a genuine, complete page load, with
  `WebKitNetworkProcess`/`WebKitWebProcess`/`WebKitGPUProcess` all actually spawned
  from the relocated `libexec/` directory.
- **LIBGL_DRIVERS_PATH**: see the dedicated section above -- tested, found not
  load-bearing for this build's actual runtime code paths.

See `task-8-report.md` (in the plan's `.superpowers/sdd/` directory, not committed to
this repo) for the full command transcripts and raw output.

## Bundle contents and what was deliberately left out

The tarball ships `include/`, `lib/`, `libexec/`, `etc/`, `share/` from the build
prefix (minus `libexec/installed-tests` and `share/installed-tests`, ~22 MB of
gdk-pixbuf/GLib "make check" test fixtures never consumed at runtime), plus one
specific binary copied out of `bin/` into `libexec/gdk-pixbuf-tools/`
(`gdk-pixbuf-query-loaders` -- see the `loaders.cache` note above). The rest of
`bin/` and all of `sbin/` (build-time CLI tools -- `glib-compile-resources`, ICU's
`genccode`, etc.) are excluded entirely: nothing else that `dlopen()`s this bundle as
an embedded WebView needs them. This is a real correction versus an earlier draft of
`package.sh`, which only tarred `include lib` and would have silently shipped a
bundle whose `WebKitWebProcess`/`WebKitNetworkProcess`/`WebKitGPUProcess` binaries
(which live under `libexec/`, not `lib/`) simply weren't in the archive at all.

Every ELF file in the shipped tree is stripped (`strip --strip-unneeded`, in
`package.sh`'s same per-file loop as the RPATH rewrite) -- a Task 8 post-review
correction, C-2: the first committed tarball came in at 160.9 MiB, over GitHub's
100 MiB hard per-file limit (`git push` would have rejected the commit outright).
Root cause was unstripped debug symbols across the meson-built half of this
dependency chain (unlike WebKitGTK's own build, which already sets
`-DCMAKE_BUILD_TYPE=Release`); stripping brought it back under the limit with no
functional loss (`--strip-unneeded` keeps the dynamic symbol table every shared
library needs to actually load/link, only removing debug info and local symbols).

Static archives (`*.a`) are excluded too -- a final-review finding (I-5). Several
dependencies install one alongside their shared library (`libnettle.a` and
`libhogweed.a` at ~18 MB each, `libpcre2-{8,16,32}.a`, `libsqlite3.a`, `libgmp.a`,
`libwebp*.a`, `liblcms2.a`, `libopenjp2.a`, `libtasn1.a`), ~50 MB uncompressed and a
measured 5.1 MiB of the compressed tarball. A static archive is link-time-only
input: nothing that `dlopen()`s this bundle at runtime can consume one, and nothing
links against this bundle at build time either (see the pkg-config note below --
the shipped `.pc` files don't survive relocation anyway). They were also invisible
to `package.sh`'s RPATH/strip loop, which skips them on the ELF-magic check
(`!<arch>`, not `\x7fELF`).

Known, not yet addressed: the tarball is still not aggressively size-optimized
beyond stripping and the `*.a` exclusion (`include/` is ~25 MB of headers, and
`share/` still carries `locale` (~28 MB) plus `man`/`info`/`gdb`/`bash-completion`/
`zsh`/`aclocal`/`gettext`/`cmake`/`pkgconfig`/`xml` subdirectories that are almost
certainly unnecessary at runtime). Left as-is per this plan's self-review notes,
which flagged tarball size as a real but deliberately-deferred concern beyond what
C-2 required -- worth a further pass once Plans 2-4 prove the bundle works
end-to-end, not before. Headroom matters here specifically because this tarball is
committed to git and GitHub's 100 MiB per-file hard limit was already hit once
(C-2); WebKitGTK grows with every release.

### Note on pkg-config against a relocated bundle

The bundle's own `.pc` files (e.g. `webkitgtk-6.0.pc`, `gio-2.0.pc`) still contain
the build's own absolute prefix (`prefix=/opt/tuxblox-webview`) baked into their
`Cflags:`/`Libs:` lines, so `pkg-config --cflags` against a relocated copy returns
nonexistent paths. Anything in Plans 2-4 that wants to build against this bundle
needs to know its layout directly rather than discover it via `pkg-config`.

## System-provided libraries (not vendored)

This bundle vendors almost its entire dependency chain from source into `lib/` --
but not everything. A small set of libraries are deliberately treated as "assumed
present on any real target Linux desktop" rather than built from source, on the
theory that they're effectively part of the Linux kernel/init-system/X-Window-System
ABI on any modern distro -- present virtually everywhere already, and unusually
impractical or pointless to vendor (some, like the X11/Wayland client libraries,
have to match whatever display server the target machine is actually running
anyway). This boundary has grown twice since the plan's original Global Constraints:

- **Foundational (established from Task 1)**: zlib (`libz`), `libpng`, `freetype`,
  `fontconfig`, `expat`, `pixman`.
- **Extended in Task 4**: X11 core + extensions (`libX11`, `libXext`, `libXrender`,
  `libXi`, `libXrandr`, `libXcursor`, `libXdamage`, `libXfixes`, `libXinerama`,
  `libXxf86vm`, the `libxcb*` family), Wayland (`libwayland-client/server/egl/cursor`),
  DRM (`libdrm`), `libepoxy`, `libxkbcommon`, `libgcrypt`, `libnghttp2`, `libtiff`,
  `libudev`.
- **Extended by the Task 8 post-review correction (2026-08-10)**: `libselinux`,
  `libmount`, `libbrotlidec`/`libbrotlienc`/`libbrotlicommon`, `libzstd`,
  `libsystemd`, `libatomic`, `libxshmfence`. Found missing by running `ldd` against
  the packaged bundle on this repo's own (Arch Linux) host machine -- outside the
  build container for the first time in this whole plan -- and accepted onto this
  same boundary for the same reasoning as the two entries above: these are
  effectively part of the Linux kernel/init-system ABI itself (SELinux, device
  mounting, compression, systemd, atomics, X11 shared-memory fence primitives), not
  anything WebKitGTK-stack-specific.
- **Also assumed present, but as external executables rather than linked
  libraries**: `bwrap` (bubblewrap) and `xdg-dbus-proxy` -- *not* currently a live
  runtime dependency, since `ENABLE_BUBBLEWRAP_SANDBOX=OFF` (see below), but their
  absolute host paths are still compiled into `libwebkitgtk-6.0.so` as
  `-DBWRAP_EXECUTABLE`/`-DDBUS_PROXY_EXECUTABLE` string literals from this
  container's own `/usr/bin`, worth knowing about if that sandbox is ever
  re-enabled in a future build.

Deliberately vendored instead of relying on this boundary, despite being common
system libraries elsewhere, because a real WebKitGTK requirement made them
unsuitable for it: `libjpeg` (Debian ships a distro-specific `libjpeg.so.62`
SONAME most other distros don't -- see `build-in-container.sh`'s libjpeg-turbo
section), `libxml2`, `libxslt`. See `verify-outside-container.sh` below for how the
distinction between "genuinely system-provided" and "actually needs vendoring" gets
tested for real rather than assumed.

## Verifying outside the build container

Every dependency-closure check in this plan, through Task 8's own original pass,
ran from *inside* the `tuxblox-webkitgtk-builder` container -- where every `-dev`
package this `Containerfile` installs has its matching runtime `.so` sitting right
there on the default library search path. That made it structurally impossible to
notice a shipped bundle depending on one of those system libraries at runtime, since
the check itself always ran somewhere those libraries happened to already be
present. This was a real, whole-plan methodology gap, not a one-off oversight --
found by Task 8's own post-review, which ran `ldd` against the packaged tarball on
this repo's actual host machine (Arch Linux, sharing essentially none of Debian's
package layout) for the first time and immediately found real, previously-invisible
bugs (`libjpeg.so.62`, `libxml2`, `libmanette`, `libhyphen`, `libselinux`, and more).

    ./verify-outside-container.sh [path/to/webkitgtk-*.tar.xz]

Runs the same dependency-closure sweep from two places that are *not* the build
container: a bare `debian:12-slim` container with nothing installed beyond what that
base image ships, and this repo's own host machine, whatever distro that happens to
be. Classifies every unresolved `NEEDED` entry against the system-provided boundary
documented above, so its output distinguishes real bugs from accepted, documented
assumptions about the target system. Run this (not just the in-container RPATH
sweep in `package.sh`) after any dependency version bump or new library addition --
it is the only check in this whole pipeline that can actually catch this class of
bug.

## Bubblewrap sandbox

WebKitGTK's process sandbox (`ENABLE_BUBBLEWRAP_SANDBOX`) is built with
`-DENABLE_BUBBLEWRAP_SANDBOX=OFF` -- see the corresponding comment in
`build-in-container.sh` and Task 7/8's reports for the full reasoning. This trades
away WebKit's process-level sandboxing for genuine host independence (no runtime
dependency on `/usr/bin/bwrap`/`/usr/bin/xdg-dbus-proxy`).

## GPU-accelerated rendering via glvnd dispatch (attempted, BLOCKED -- see report)

An attempt was made to let the host's own glvnd dispatch pick a real GPU vendor
(e.g. NVIDIA proprietary) instead of always using this bundle's own
`-Dgallium-drivers=softpipe` Mesa, for hosts that have one. **Reverted -- two real,
independently-confirmed blockers, not a "didn't get to it":**

1. NVIDIA's proprietary `libEGL_nvidia.so` genuinely SEGFAULTs (confirmed via
   `coredumpctl`, real backtrace, reproduced twice) inside its own internal
   `dlopen()` call the first time it's loaded -- even transitively, via libepoxy's
   own lazy `dlopen("libEGL.so.1")` -- inside `unixlib.c`'s isolated
   `dlmopen(LM_ID_NEWLM)` namespace. Closed-source driver code; not fixable from
   this bundle or from `unixlib.c`.
2. Independently of (1): `setenv("LD_LIBRARY_PATH", ...)` called at DLL-init time
   from inside webview2loader.so (already running deep inside an already-started
   Wine process) does NOT affect glibc's `dlopen()` search path for that process --
   glibc parses `LD_LIBRARY_PATH` into its internal search list once, at the
   process's own startup, before this DLL's init code ever runs. An
   env-var-at-DLL-init-time fallback mechanism cannot work for this specific
   injection point, regardless of (1).

Full evidence, what was tried, and the most promising remaining lead (deciding the
fallback before Wine's own process starts, e.g. in `launch.sh`, rather than from
inside `unixlib.c`) are in
`.superpowers/sdd/2026-08-13-webview2-window-docking-messaging/lag-glvnd-report.md`.
No source changes from this attempt are committed -- this bundle's own Mesa softpipe
build is unchanged, and `unixlib.c` is unchanged.

---

# Carried forward: unresolved issues for later plans

Everything below was found by the final whole-branch review of this bundling work
(2026-08-10). None of it is a defect in the tarball this directory produces -- the
bundle itself verifies clean -- but each one is a real decision or obligation that
lands on whoever does the actual Wine integration (Plans 2-4) or cuts the first
release that ships this bundle. Written down here rather than left in a review
transcript, because none of it is discoverable from the code alone.

## LD_LIBRARY_PATH beats this bundle's RUNPATH (RESOLVED in Task 1)

**This collision was the blocker identified in Plan 1 as required-before-Plan-2.
Fixed by applying `patchelf --force-rpath` in Task 1.**

`ProtonSource/proton` (lines 1519-1520) prepends `<dist>/lib/x86_64-linux-gnu`
(plus the aarch64/i386 siblings) to `LD_LIBRARY_PATH` for every wine process it
launches. In `ld.so`'s search order, `LD_LIBRARY_PATH` is consulted **before**
`DT_RUNPATH`. The issue: `patchelf --set-rpath` writes `DT_RUNPATH`, not `DT_RPATH`,
so Proton's own libraries would win over this bundle's for any soname they both provide.

They provide a lot of the same sonames. Measured against a real build:

- **72 files / ~36 distinct libraries overlap** between this bundle and Proton's own
  `dist/files/lib/x86_64-linux-gnu`: the entire native-Linux GStreamer stack
  (`libgstreamer-1.0.so.0`, `libgstbase/app/audio/video/gl/pbutils/codecparsers/...`)
  plus `libgraphene-1.0.so.0`.
- **The versions differ**: Proton ships GStreamer 1.29.x (`libgstreamer-1.0.so.0.2902.0`)
  and graphene 1.11.x; this bundle ships GStreamer 1.26.5 and graphene 1.10.8.
- **Proton's `libgstreamer-1.0.so.0.2902.0` has no RUNPATH at all** and lists
  `libglib-2.0.so.0` in its `NEEDED` entries. `ld.so` deduplicates by soname, so
  whichever `libglib-2.0.so.0` gets loaded first wins **for everything in that process**.

**Fix applied in Task 1:** `package.sh` now uses `patchelf --force-rpath --set-rpath`
instead of plain `--set-rpath`. `DT_RPATH` is consulted *before* `LD_LIBRARY_PATH`
and is inherited transitively by dependencies that lack their own, making the bundle's
libraries win by default. `DT_RPATH` is formally deprecated but universally supported.

**Verification performed:**

```bash
# Step 1: Confirmed DT_RPATH (not DT_RUNPATH) is set on the bundle's libraries
readelf -d out/extract/lib/libwebkitgtk-6.0.so.4* | grep -E 'RPATH|RUNPATH'
# Expected: (RPATH) only, no (RUNPATH)

# Step 2: Simulated Proton's LD_LIBRARY_PATH prepending with a decoy library
mkdir -p /tmp/decoy/lib
echo 'not a real library' > /tmp/decoy/lib/libgstreamer-1.0.so.0
LD_LIBRARY_PATH=/tmp/decoy/lib:$LD_LIBRARY_PATH \
  ldd out/extract/lib/libwebkitgtk-6.0.so.4* | grep gstreamer
rm -rf /tmp/decoy
# Expected: resolved path points inside out/extract/lib/..., not /tmp/decoy/lib
# (Before the fix, with plain --set-rpath, the decoy would win.)
```

Verified that with `DT_RPATH` in place, the bundle's own libraries win even when
a decoy is prepended to `LD_LIBRARY_PATH`, confirming the fix handles the exact
collision Proton introduces.

## gdk-pixbuf `loaders.cache` is relocated a second time by the installer (UNRESOLVED)

`ProtonSource/Makefile.in`'s extraction rule regenerates `loaders.cache` after
unpacking, so its baked-in absolute loader paths match the real extraction directory
(see the `loaders.cache` note earlier in this file). That fixes exactly one
relocation -- the one from this bundle's `/opt/tuxblox-webview` build prefix to the
Proton build's `$(DST_DIR)/lib/tuxblox-webview`.

**There is a second relocation the fix does not account for.**
`installer/src/installer_steps.cpp` packs the finished Proton build as
`protonbuild-*.tar.zst` and extracts it to `~/.tuxblox/proton` on the end
user's machine. The `loaders.cache` regenerated at Proton-build time therefore
contains the *build machine's* absolute paths (e.g.
`/home/<maintainer>/.../build/proton/files/lib/tuxblox-webview/...`), which do
not exist on any user's system -- reintroducing precisely the stale-absolute-path
bug the Makefile step was written to solve, one hop later.

Impact is bounded but real: only the GIF and TIFF loaders ship as separate modules
(PNG and JPEG are compiled into `libgdk_pixbuf-2.0.so.0` itself -- it links
`libpng16`/`libjpeg.so.8` directly), so the visible effect is those two formats
failing to decode through gdk-pixbuf, not a hard failure. It will get worse if a
future gdk-pixbuf ships more loaders as modules.

Needs a decision in a later plan or an installer change; both options are
straightforward:

- Regenerate the cache at **install time or first launch** (the tarball already ships
  `libexec/gdk-pixbuf-tools/gdk-pixbuf-query-loaders` for exactly this purpose), or
- set **`GDK_PIXBUF_MODULE_FILE`** at runtime to a cache generated into a writable
  per-user location, alongside the other environment variables in the contract table
  above.

## Supply-chain hardening gap (known limitation, not blocking)

Nothing in this pipeline verifies what it downloads:

- `build-in-container.sh` fetches ~28 source tarballs with a bare `curl -fL` and no
  checksum or signature check. `versions.env` pins *versions*, which protects against
  upstream drift but not against a compromised or substituted mirror.
- `Containerfile` uses the moving `FROM debian:12` tag rather than a digest, and
  `pip3 install --break-system-packages --upgrade 'meson>=1.4.0'` is unpinned -- so
  two runs months apart can legitimately produce different toolchains.

For a component that renders live web content and handles Roblox login sessions,
this is worth closing before the bundle ships in an actual release: add a
`sha256sums` file next to `versions.env` and verify it inside `fetch_and_extract`,
pin the base image by digest, and pin the meson version exactly. Not blocking today
(the artifact in `ProtonSource/contrib/` was built and verified by hand), but it is
the difference between "pinned" and "reproducible", and the whole point of this
directory is that someone can rebuild it in six months for a security update and
trust the result.

## Third-party license attribution needed before release

This bundle vendors roughly 28 libraries that were not previously part of anything
TuxBlox ships -- WebKitGTK, GTK-4, GLib, GnuTLS (with LGPLv3 `libunistring`
compiled in), Nettle/Hogweed, GMP, libsecret, GStreamer, Mesa, ICU, SQLite,
libsoup, Cairo, Pango, HarfBuzz, gdk-pixbuf, libxml2/libxslt, libjpeg-turbo,
libwebp, and others -- almost all LGPL or similarly notice-bearing. Redistributing
them in a release carries attribution obligations (license text + notice, and for
the LGPL components, source availability). TuxBlox also *modifies* WebKit: the
`WEBKIT_EXEC_PATH` patch applied in `build-in-container.sh`.

The repo already has machinery for this -- `third_party_licenses/` and
`installer/cmake/EmbedThirdPartyLicenses.cmake`, which generates the installed
product's `COPYRIGHT.txt` -- but it currently covers only Proton and the installer's
own vendored dependencies, not anything in this bundle.

**Deliberately not done here:** `CLAUDE.md` states `third_party_licenses/` holds
license texts for bundled dependencies and must not be modified, so this is flagged
for the repo owner to handle rather than changed unilaterally. It should be resolved
before any release ships this bundle, not before this work merges.
