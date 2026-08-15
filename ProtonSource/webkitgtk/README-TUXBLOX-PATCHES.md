# TuxBlox patches to WebKitGTK

This tree is WebKitGTK 2.52.5 (see `versions.env` in `webkitgtk-bundle/`), checked in
directly rather than downloaded and patched on every build -- same treatment
`ProtonSource/wine` already gets, and for the same reason (see the top of
`ProtonSource/.gitmodules`): TuxBlox carries real patches against it, and applying
those as fragile inline `sed`/text-replacement scripts against a freshly-fetched
tarball on every build is both harder to review and prone to silently breaking the
moment upstream reflows a comment near the patched text.

Do not modify this tree except to (a) bump it to a newer upstream release, or (b) add/
adjust a TuxBlox patch. Every TuxBlox patch is marked inline with a `// TuxBlox patch:`
or `// TuxBlox patch (...)` comment at its exact location, so `grep -rn "TuxBlox patch"`
finds all of them. Listed here for a higher-level summary; the inline comment at each
site is the authoritative explanation.

## 1. `WEBKIT_EXEC_PATH` works in release builds

**File:** `Source/WebKit/Shared/glib/ProcessExecutablePathGLib.cpp`,
`findWebKitProcess()`

Upstream gates the `WEBKIT_EXEC_PATH` environment-variable override behind
`#if ENABLE(DEVELOPER_MODE)`. This bundle deliberately does not build with
`-DDEVELOPER_MODE=ON` (broad blast radius: also flips `DEVELOPER_MODE_FATAL_WARNINGS`
on and changes several other `ENABLE_*` defaults), but `WEBKIT_EXEC_PATH` is the only
available *runtime* override for the compile-time `PKGLIBEXECDIR` absolute-path
constant -- which patchelf/RPATH rewriting cannot touch -- and this bundle's real
install location isn't known until TuxBlox's own top-level `build.sh` runs, long after
this container image was built. Ungated only this one check; every other
`DEVELOPER_MODE`-gated branch in the same function (the `getExecutablePath()`
dev-convenience fallback) is untouched.

Originally discovered and verified during Task 8 of the
`2026-08-14-webview2loader-host-process` plan: `strings` against the as-built (pre-
patch) library confirmed the `WEBKIT_EXEC_PATH` `getenv()` call was compiled out
entirely.

## 2. `WindowIsActive` forced active for the reparented webview

**File:** `Source/WebKit/UIProcess/API/gtk/WebKitWebViewBase.cpp`,
`webkitWebViewBaseSetToplevelOnScreenWindow()`

`webview2loader-host`'s `GdkSurface` starts life as a real, independent X11 top-level,
then gets `XReparentWindow`'d into Roblox Studio's own window (see
`webkitgtk-bundle/host/geometry.c`) so it can be genuinely docked rather than floating
on top -- but once reparented, it can never receive real WM-mediated active/focus
state again: ICCCM/EWMH's `_NET_ACTIVE_WINDOW` protocol (what `gtk_window_is_active()`
and this file's own `ToplevelWindow::isActive()` ultimately read) only ever names
actual top-level windows the window manager manages, never an arbitrary reparented
child. `ActivityState::WindowIsActive` tracking is therefore permanently wrong for
this webview, and WebCore reduces/suspends its own compositing for a view that
believes its window isn't active.

Upstream already has the exact right escape hatch for this class of problem --
originally written for Xvfb, which also can't deliver real toplevel focus -- gated
behind `#if ENABLE(DEVELOPER_MODE)`. Same fix shape as patch 1: ungated only this one
check, renamed from `UNDER_XVFB` to `WEBVIEW2LOADER_FORCE_WINDOW_ACTIVE` since this
isn't Xvfb and a future reader grepping "XVFB" in this codebase would be misled
otherwise. `webview2loader-host`'s own `main.c` sets this env var unconditionally.

Found 2026-08-15, during a real flicker investigation (the reparented webview
intermittently painting solid white, worst on pointer/focus leaving its area).
Confirmed alone insufficient -- see patch 3.

## 3. `isInMonitor()` forced true for the reparented webview

**File:** `Source/WebKit/UIProcess/API/gtk/ToplevelWindow.cpp`, `isInMonitor()`

Same underlying mechanism as patch 2, one signal further down:
`webkitWebViewBaseUpdateVisibility()` in `WebKitWebViewBase.cpp` only sets
`ActivityState::IsVisible` when `ToplevelWindow::isInMonitor()` is also true. GTK4
tracks monitor membership via real `enter-monitor`/`leave-monitor` `GdkSurface`
signals (`ToplevelWindow::connectSurfaceSignals()`, same file), computed by comparing
the surface's position against monitor geometry in root-relative screen coordinates.
Once `XReparentWindow` makes this surface a child of Roblox Studio's own window, X11
`ConfigureNotify` events for it report parent-relative coordinates instead, which
GDK's own tracking has no way to know to reinterpret -- the same "reparenting breaks a
mechanism built for real top-levels" class of bug as patch 2. If monitor tracking
miscomputes and `m_monitors` empties out, `isInMonitor()` goes false, `IsVisible` gets
cleared, and an invisible page not painting is exactly consistent with the observed
solid-white/blank symptom.

Forced true under the same `WEBVIEW2LOADER_FORCE_WINDOW_ACTIVE` env var as patch 2 (no
new env var -- same underlying justification: this reparented child's real visibility
already tracks Roblox Studio's own top-level, which real GDK tracking has no way to
see). The real (pre-override) value is still computed and logged once via
`g_printerr()` the first time it would have been false, as a diagnostic signal for
whether this is really a contributing cause, independent of whether the override alone
turns out to fully fix the visible symptom.

Confirmed via that same diagnostic during a real repro: `m_monitors` never actually
emptied out, ruling this one out cleanly -- see patch 4.

## 4. `isSuspended()` forced false for the reparented webview

**File:** `Source/WebKit/UIProcess/API/gtk/ToplevelWindow.cpp`, `isSuspended()`

Same `webkitWebViewBaseUpdateVisibility()` gate as patches 2 and 3, one more input:
`IsVisible` also requires `!ToplevelWindow::isSuspended()`, which reads
`GDK_TOPLEVEL_STATE_SUSPENDED` off the surface's own `notify::state`-tracked state
(`ToplevelWindow::connectSurfaceSignals()`, same file) -- a real GTK 4.12+ compositor
hint meaning "this surface doesn't need to render." Same reasoning as patches 2/3:
whatever X11-crossing/state-tracking machinery computes this for a normal top-level
has no defined behavior for a window that's been `XReparentWindow`'d into someone
else's hierarchy, and this specific fix targets the repo owner's own refined repro
(the webview content disappears specifically when the pointer leaves Roblox Studio's
entire window, not just the smaller webview sub-rectangle within it -- consistent with
an X11 crossing event delivered up through the whole ancestor chain on exit, which is
exactly the kind of event `notify::state`/toplevel-state tracking reacts to).

Same diagnostic-first shape as patch 3: real value still computed and logged once via
`g_printerr()` before being overridden, so a real test confirms or rules this out
regardless of whether the override alone fixes the visible symptom.

## Rebasing onto a new upstream WebKitGTK release

1. Download and extract the new release tarball to a scratch directory (`tar --strip-components=1`).
2. Diff the three files above (in the scratch extraction) against this tree's copies to
   confirm the patched regions haven't moved/changed shape upstream.
3. Re-apply each patch (or adjust it, if upstream changed the surrounding code) to the
   new source.
4. Replace this tree's contents with the new, patched source (keep this file).
5. Bump `WEBKITGTK_VERSION` in `webkitgtk-bundle/versions.env`.
6. Update `third_party_licenses/webkitgtk/` if any dependency version pinned there also
   moved (see that directory's own `SOURCES.md`).
