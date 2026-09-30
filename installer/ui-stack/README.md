# ui-stack

Builds the shared GTK4 and libadwaita stack that TuxBlox's graphical interface links against, from source, inside the sniper SDK container (glibc 2.31).

```
./build.sh
```

Outputs land in `build/.artifacts/ui-stack/`:

- `ui-stack-<version>-x86_64.tar.zst` holds `lib/`, `share/glib-2.0/schemas/` and `fonts/`. Every library's RPATH is `$ORIGIN`-relative, so the tree resolves against itself wherever it is unpacked.
- `dev/` holds `include/` and `lib/pkgconfig/` (with the libraries under `lib/x86_64-linux-gnu/`) for compiling against. Its `.pc` files locate themselves through `${pcfiledir}`.

The build is resumable: each library leaves a marker in `prefix/.done/`, so a rerun skips what already finished. Delete a marker to rebuild that library.

Notes:

- Wayland and wayland-protocols are built here because the SDK's copies are older than GTK 4.18 needs, and GTK silently drops its Wayland backend when they are missing. `build-in-container.sh` fails the build unless `libgtk-4.so` exports both `gdk_wayland` and `gdk_x11` symbols.
- libtiff is built here (zlib only) because GTK requires it and the host's soname differs between distributions.
- Mesa is deliberately not built, so the host's GL driver is used.
- libadwaita is patched (`patch-libadwaita.py`) to drop its AppStream dependency, which only serves `adw_about_dialog_new_from_appdata`.
- Host libraries still linked: X11/xcb, basics (zlib, libpng, libjpeg, expat, pixman, fribidi, libthai, brotli, pcre2).
