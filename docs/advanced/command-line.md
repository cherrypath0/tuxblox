# Command Line Reference

Every TuxBlox program you can run from a terminal, and what its options do.

Nothing here is required for normal use. This page exists for scripting, for headless machines, and for people who would rather not click.

---

## TuxBloxLauncher

`~/.tuxblox/TuxBloxLauncher`

Run with no arguments, it opens the launcher window. With an argument, it does one specific thing and exits.

| Argument | What it does |
|---|---|
| *(none)* | Opens the launcher window |
| `--launch-studio` | Starts Roblox Studio directly, no window |
| `--launch-player` | Starts Roblox Player directly, no window |
| `--open-file <path>` | Opens a `.rbxl` or `.rbxlx` file in Studio |
| `roblox:…` | Starts Player with that link |
| `roblox-player:…` | Same |
| `roblox-studio:…` | Starts Studio with that link |
| `roblox-studio-auth:…` | Signs in to Studio from a website link |

Two more exist but are meant for TuxBlox's own use rather than yours:

| Argument | What it does |
|---|---|
| `--watch-launch player\|studio` | Starts Roblox and stays running to watch the session |
| `--run-exe <path>` | What the exported desktop shortcuts use |

`--run-exe` only accepts `RobloxStudioBeta.exe` or `RobloxPlayerBeta.exe` and refuses anything else. It ignores the rest of the path and works out which Roblox to start from the filename, which is why an old shortcut keeps working after Roblox updates.

### Examples

```bash
# Start Studio from a script
~/.tuxblox/TuxBloxLauncher --launch-studio

# Open a place file
~/.tuxblox/TuxBloxLauncher --open-file ~/Places/MyGame.rbxl
```

> [!NOTE]
> Only one launcher **window** can be open at a time. The direct launch options above are not affected by that, so you can use them while the window is open.

---

## TuxBloxInstaller

`~/.tuxblox/TuxBloxInstaller`

Installs TuxBlox into `~/.tuxblox`, upgrading an existing install in place if it finds one, then starts the launcher.

```
Usage: TuxBloxInstaller [options]

  --headless         Report progress on the terminal instead of
                     opening a window. Needs no display.
  --nolaunch         Don't start the launcher once the install
                     finishes.
  --uninstall        Remove TuxBlox from this system instead of
                     installing it.
  --channel <name>   Release channel to install from (default: stable).
  --version          Show the build version and exit.
  -h, --help         Show this help and exit.
```

Unknown options are an error rather than being ignored, so a typo cannot quietly change what the installer does.

### Examples

```bash
# Install over SSH, with no desktop
./TuxBloxInstaller --headless --nolaunch

# Install the canary channel
./TuxBloxInstaller --channel canary

# Remove TuxBlox completely
~/.tuxblox/TuxBloxInstaller --uninstall
```

---

## The compatibility layer

`~/.tuxblox/compat/main`

This is the layer itself. The launcher drives it for you, and you will rarely call it directly, but it is a normal program you can run.

```
Options:
  --help                  Show this help message
  --version               Show TuxBlox version
  --immediate             Run TuxBlox without draining the prefix, must be
                          used with the "run" argument
  --destroy               Destroys the prefix

Arguments:
  run <executable>        Runs the specified executable
```

It needs to be told where the virtual drive is, through `TUXBLOX_PREFIX`:

```bash
TUXBLOX_PREFIX=~/.tuxblox/runtime ~/.tuxblox/compat/main --version
```

### run and --immediate

`run` starts a program and then waits for the virtual drive to empty out, meaning every Windows process inside it has exited. That is what you want for Roblox itself.

`run --immediate` skips the wait. Use it for anything that should return as soon as its own process ends, such as a helper running alongside Studio. [Studio MCP](../using-tuxblox/studio-mcp.md) is exactly this case.

### --destroy

Deletes the virtual drive. Same effect as **Wipe prefix** in the launcher's settings, which is the friendlier way to do it.

### A trap worth knowing

A Windows executable has to live inside the virtual drive's `C:` to be runnable. Pointing `run` at an executable that sits elsewhere on your Linux filesystem fails with **exit code 139 and no output at all**, which looks exactly like a crash and is not one.

Copy it in first:

```bash
cp probe.exe ~/.tuxblox/runtime/pfx/drive_c/
~/.tuxblox/compat/main run "$HOME/.tuxblox/runtime/pfx/drive_c/probe.exe"
```

The path must be absolute. A relative one fails differently, with `wine: failed to open`.

> [!TIP]
> Read the program's output through a pipe, not a file redirect. Redirecting straight to a file gives the Windows program empty output handles and you end up with a zero byte file, which looks like the program printed nothing.

---

## mcp.sh

`~/.tuxblox/mcp.sh`

Runs Roblox's Studio MCP server through the compatibility layer. Takes no options of its own and passes anything it is given to the server.

This is what you point an AI client at. Use the absolute path, since most MCP clients will not expand `~`.

See [Studio MCP](../using-tuxblox/studio-mcp.md).
