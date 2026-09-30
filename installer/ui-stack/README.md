<!--
TuxBlox - Linux Compatibility Layer for the Roblox Engine
Copyright (C) 2026 TuxBlox Developers

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <https://www.gnu.org/licenses/>.
-->

# ui-stack

Builds the shared GTK4 and libadwaita stack that TuxBlox's graphical interface links against, from source, inside the sniper SDK container (glibc 2.31).

```
./build.sh
```

Everything lands in `dist/` next to this file (gitignored). It is deliberately not under the repository's `build/`, which the root `build.sh` wipes: the stack takes over an hour to build and is an input to that build, not an output of it.

- `dist/ui-stack-<version>-x86_64.tar.zst` holds `lib/`, `share/glib-2.0/schemas/` and `fonts/`. Every library's RPATH is `$ORIGIN`-relative, so the tree resolves against itself wherever it is unpacked.
- `dist/dev/` holds `include/` and `lib/pkgconfig/` (with the libraries under `lib/x86_64-linux-gnu/`) for compiling against. Its `.pc` files locate themselves through `${pcfiledir}`.
- `dist/prefix/` and `dist/work/` are the build's install prefix and source trees. Each library leaves a marker in `dist/prefix/.done/`, so a rerun skips what already finished. Delete a library's marker to rebuild only that library; it re-downloads its own source if `dist/work/` was cleared.

`package.sh` fails the build when any shipped library has a `DT_NEEDED` entry that is neither in the stack nor on its short allow-list of libraries every Linux desktop has (libc, X11/xcb, GL/EGL, zlib, libpng, expat, pcre2, fontconfig, and a few more). Adding a library to that list is a decision about what the stack assumes of the user's machine.

Notes:

- Wayland and wayland-protocols are built here because the SDK's copies are older than GTK 4.18 needs, and GTK silently drops its Wayland backend when they are missing. `build-in-container.sh` fails the build unless `libgtk-4.so` exports both `gdk_wayland` and `gdk_x11` symbols.
- libtiff, libjpeg-turbo and pixman are built here because their sonames differ between distributions (or, for pixman, they are not guaranteed to exist without cairo installed).
- Pango is built without libthai and freetype without brotli, since both would otherwise be extra host dependencies.
- Mesa is deliberately not built, so the host's GL driver is used.
- libadwaita is patched (`patch-libadwaita.py`) to drop its AppStream dependency, which only serves `adw_about_dialog_new_from_appdata`; the two `new_from_appdata` constructors become stubs that log a critical message and return an empty dialog.
