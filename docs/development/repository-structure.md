# Repository Structure

A tour of the TuxBlox repository, for anyone about to change something in it.

## Two halves, two licenses

This is the most important thing to understand before touching anything.

| Part | License |
|---|---|
| Everything outside `compat/` | **GPLv3** |
| `compat/` | **LGPLv2.1**, inherited from Wine and Proton |
| `compat/tuxblox/` and `compat/webkitgtk/bundle/` | **GPLv3**, TuxBlox's own code inside the layer |
| `third_party_licenses/` | Whatever each bundled project uses. Do not modify. |

The two halves are **separate compiled programs that talk to each other while running**, not one program linked from both. That separation is exactly what lets the project carry two licenses cleanly, so keep it intact in any change you make.

## Top level folders

| Folder | What is in it |
|---|---|
| `launcher/` | Source for `TuxBloxLauncher`, the window you use |
| `installer/` | Source for `TuxBloxInstaller` |
| `compat/` | The compatibility layer |
| `include/` | Files copied into the finished build as-is, such as `mcp.sh` |
| `docs/` | This documentation |
| `third_party_licenses/` | License texts for everything bundled |

## Top level files

| File | What it is |
|---|---|
| `build.sh` | Builds the installer, launcher and compatibility layer, then packages the result |
| `launch.sh` | Runs what you just built, without installing it |
| `VERSION` | The version and channel. One file, read by all three components. |
| `Containerfile` | The container builds run inside, so they are the same on every machine |
| `CONTRIBUTING.md` | How to contribute |
| `SECURITY.md` | How to report a security issue |
| `LICENSE` | The GPLv3 text |

## Inside compat/

The compatibility layer is by far the largest part of the repository.

| | |
|---|---|
| `compat/wine/` | TuxBlox's Wine fork. Checked in directly and maintained here, not a submodule. |
| `compat/submodules/` | Around two dozen dependencies, including DXVK and vkd3d |
| `compat/tuxblox/` | TuxBlox's own C++ code for the layer, including the entry point |
| `compat/webkitgtk/bundle/` | TuxBlox's own browser component, used for Studio's login screen and Toolbox |
| `compat/patches/` | Changes TuxBlox applies to submodule sources |
| `compat/make/` | The build rules |

### How patches work

TuxBlox does not commit changes into the dependency submodules. Instead, a change lives in `compat/patches/<package>/<path>`, and the build copies it over its own copy of the source.

The submodules stay clean, which means updating one does not fight with local edits, and nothing has to be committed inside a submodule.

The one exception is `compat/wine/`, which is a fork TuxBlox maintains, so it is edited in place like any other source.

## Build output

`build/` holds everything a build produces and is not tracked in git.

```
build/
├── compat/          the compatibility layer, entry point is main
├── libtuxblox/      libraries the launcher's interface needs
├── runtime/         the virtual drive
├── TuxBloxLauncher
├── TuxBloxInstaller
├── mcp.sh
└── .artifacts/      intermediate build files
```

That layout deliberately matches what ends up in `~/.tuxblox`, so what you test is shaped like what ships.

## Where to put a change

| You want to change | Go to |
|---|---|
| Something in the launcher window | `launcher/src/ui_qt/` |
| How a setting behaves | `launcher/src/settings.cpp` |
| The install or update flow | `installer/src/` |
| How Roblox is started | `compat/tuxblox/src/launch/` |
| How the virtual drive is built | `compat/tuxblox/src/prefix/` |
| Windows behaviour Roblox depends on | `compat/wine/` |
| Studio's login screen or Toolbox | `compat/webkitgtk/bundle/` |
| A dependency such as DXVK | `compat/patches/<package>/` |

## Before you write code

Read [Contributing](contributing.md). It covers code style, the copyright header every TuxBlox source file needs, and what kinds of contribution are and are not accepted.
