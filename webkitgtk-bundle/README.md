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
clean run -- expect multiple hours. A re-run against a warm `webkitgtk-ccache` volume
(e.g. after only touching the WebKitGTK step itself, not any of its dependencies) is
much faster since `ccache` is wired into the WebKitGTK cmake invocation specifically
(see the comment in `build-in-container.sh`). Produces
`out/webkitgtk-<version>-x86_64.tar.xz`. Copy it into `../ProtonSource/contrib/`,
update the `WEBKITGTK_VER`/tarball filename in `../ProtonSource/Makefile.in` to match,
and commit both together.

`build.sh` uses two persistent podman named volumes so a failed/interrupted run
doesn't force starting over from nothing:
- `webkitgtk-prefix` -- the actual `/opt/tuxblox-webview` install prefix. This is what
  `package.sh` packages. Safe to delete and let `build-in-container.sh` recreate it if
  you want a genuinely clean build.
- `webkitgtk-ccache` -- compiler cache for the WebKitGTK step only. Not part of the
  shippable artifact; safe to delete any time, just makes the next build slower.

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

| Variable | What it fixes | Value to set it to | Status |
|---|---|---|---|
| `WEBKIT_EXEC_PATH` | Where `libwebkitgtk-6.0.so` looks for `WebKitWebProcess`/`WebKitNetworkProcess`/`WebKitGPUProcess` | `<extract-dir>/libexec/webkitgtk-6.0` | **Required.** Verified end-to-end (real page load fails without it, succeeds with it). |
| `WEBKIT_INJECTED_BUNDLE_PATH` | Where `libwebkitgtk-6.0.so` looks for the web-process injected bundle (`libwebkitgtkinjectedbundle.so`) | `<extract-dir>/lib/webkitgtk-6.0/injected-bundle` | **Required** to avoid a load-time warning (page loads either way, but the injected bundle -- used for WebKit's internal JS-side extension hooks -- silently fails to load without it). Not in the original brief; found during WEBKIT_EXEC_PATH verification. |
| `GIO_EXTRA_MODULES` | GIO's TLS backend module (glib-networking's GnuTLS module) | `<extract-dir>/lib/x86_64-linux-gnu/gio/modules` | **Required.** Verified: `GTlsBackendDummy` (no TLS at all) without it, `GTlsBackendGnutls` (real) with it. |
| `GBM_BACKENDS_PATH` | Mesa's GBM backend search path (`libgbm`) | `<extract-dir>/lib/x86_64-linux-gnu/gbm` | **Required.** Verified against a real DRM render node: `gbm_create_device()` fails (logs the exact compiled-in `/opt/tuxblox-webview/...` path it tried) without it, succeeds with it. |
| `LIBGL_DRIVERS_PATH` | Mesa's DRI driver search path (`libGL`/`libEGL`) | `<extract-dir>/lib/x86_64-linux-gnu/dri` | **Not load-bearing for this specific build** (see below) -- set anyway, it's free insurance. |

where `<extract-dir>` is the tarball's own root (i.e. what's inside the `webkitgtk/`
top-level directory the tarball's `--transform` wraps everything in -- for
`ProtonSource/Makefile.in`'s extraction rule, that's `$(DST_DIR)/lib/tuxblox-webview`
after `--strip-components=1`).

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
$ grep -rl 'LIBGL_DRIVERS_PATH' lib/ libexec/
(no matches -- exit 1)
```

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
gdk-pixbuf/GLib "make check" test fixtures never consumed at runtime). `bin/` and
`sbin/` (build-time CLI tools -- `glib-compile-resources`, ICU's `genccode`, etc.)
are excluded entirely: nothing that `dlopen()`s this bundle as an embedded WebView
needs them. This is a real correction versus an earlier draft of `package.sh`, which
only tarred `include lib` and would have silently shipped a bundle whose
`WebKitWebProcess`/`WebKitNetworkProcess`/`WebKitGPUProcess` binaries (which live
under `libexec/`, not `lib/`) simply weren't in the archive at all.

Known, not yet addressed: the tarball is not yet aggressively size-optimized (`share/`
still carries `man`/`info`/`gdb`/`bash-completion`/`zsh`/`aclocal`/`gettext`/`cmake`/
`pkgconfig`/`xml` subdirectories that are almost certainly unnecessary at runtime).
Left as-is per this plan's self-review notes, which flagged tarball size as a real
but deliberately-deferred concern -- worth a pass once Plans 2-4 prove the bundle
works end-to-end, not before.

## Bubblewrap sandbox

WebKitGTK's process sandbox (`ENABLE_BUBBLEWRAP_SANDBOX`) is built with
`-DENABLE_BUBBLEWRAP_SANDBOX=OFF` -- see the corresponding comment in
`build-in-container.sh` and Task 7/8's reports for the full reasoning. This trades
away WebKit's process-level sandboxing for genuine host independence (no runtime
dependency on `/usr/bin/bwrap`/`/usr/bin/xdg-dbus-proxy`).
