# TuxBlox Installer

A standalone graphical installer for [TuxBlox](https://tuxblox.net). It downloads and installs the compiled Proton build, the TuxBlox Launcher, and itself into `~/.local/share/tuxblox/`, then hands off to the Launcher. Run against an existing install, it auto-detects that (no separate flag needed) and switches into upgrade mode: it replaces only `ProtonBuild/` and the Launcher/Installer binaries, leaving `runtime/` (the Wine prefix, including the user's Roblox login) and everything else untouched, showing "Upgrading Proton"/"Upgrading TuxBlox" instead of the fresh-install wording. This is also how the Launcher applies its own updates -- it doesn't download or apply anything itself, it verifies/fetches this same persisted `TuxBloxInstaller` binary and execs it. It does not handle repair or uninstall.

Built with [Dear ImGui](https://github.com/ocornut/imgui) + SDL2 + OpenGL3, in C++17.

## Building

```bash
./build.sh
```

This detects your package manager (apt/dnf/pacman/brew/apk) and installs the required system packages (`cmake`, a C++ compiler, `SDL2`, `libcurl`, `libarchive`, `openssl`, `rsvg-convert`), vendors the third-party dependencies below via `./vendor.sh`, then configures and builds. The resulting binary is at `build/TuxBloxInstaller`.

## Testing

```bash
cd build && ctest
```

Covers the non-UI logic: manifest parsing, checksum verification, download handling, tar extraction, install path resolution, and progress tracking. There's no automated coverage of the ImGui rendering code (`src/ui.cpp`), changes there are verified by building and running the installer.

## Dependencies

Vendored automatically by `./vendor.sh` into `third_party/` (not committed to this repo, automatically created by `vendor.sh`):

- [Dear ImGui](https://github.com/ocornut/imgui) (MIT) pinned to `v1.91.0`
- [nlohmann/json](https://github.com/nlohmann/json) (MIT) pinned to `v3.11.3`, single-header
- [stb_image.h](https://github.com/nothings/stb) (MIT / public domain) pinned to a fixed commit

The Inter font (OFL-1.1) is fetched and embedded at build time by `cmake/FetchFont.cmake`, and the TuxBlox logo is fetched and embedded by `cmake/FetchLogo.cmake`, neither needs a separate vendoring step.

## License

This installer is licensed under the same license as the rest of the repository. See the [`LICENSE`](../LICENSE) file at the project root.

It bundles the third-party components listed above, each under its own license. Rather than duplicating those license texts in this repository, the installer embeds them at build time and writes them out as `COPYRIGHT.txt` alongside the installed product (`~/.local/share/tuxblox/COPYRIGHT.txt`). See `src/copyright_file.cpp`.
