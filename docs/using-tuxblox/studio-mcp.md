# Studio MCP

Roblox ships an MCP server for Studio, which lets an AI assistant read and change your place while Studio is open. It is a Windows program, so on Linux it needs to run through TuxBlox.

`studio-mcp` is the program that does that. It sits in your TuxBlox folder at `~/.tuxblox/studio-mcp` and is installed with everything else.

> [!NOTE]
> **MCP** stands for Model Context Protocol. It is a standard way for AI tools to talk to applications. If you are not using an AI assistant with Studio, you can skip this page entirely.

> [!IMPORTANT]
> **`mcp.sh` is gone as of 2.7.0.** Earlier versions shipped a shell script at `~/.tuxblox/mcp.sh`. It has been replaced by `studio-mcp`, it receives no further updates, and updating to 2.7.0 removes it. If you set up Studio MCP before 2.7.0, your AI client still points at the old path and will fail to start the server until you change it. [Point it at the new one](#the-command) and restart the client.

## Before anything else: set up Studio's side

`studio-mcp` on its own is not enough. Roblox's MCP server has a Studio side that has to be installed and switched on first, and nothing will connect until it is.

**Follow Roblox's own guide: [create.roblox.com/docs/studio/mcp](https://create.roblox.com/docs/studio/mcp)**

That covers installing the server, the Studio plugin it needs, and the permission you have to grant inside Studio. Do it with Studio running under TuxBlox, so everything lands inside the virtual drive where TuxBlox can find it.

Once Roblox's guide tells you to configure your AI client, come back here. That is the part that differs on Linux.

## What you need

1. **Roblox Studio installed through TuxBlox**, and started at least once.
2. **Roblox's Studio MCP server installed**, following the guide above. TuxBlox does not ship it and does not download it.
3. **An AI client that speaks MCP.**

Roblox supports these clients:

- Claude Code
- Claude Desktop
- Codex CLI
- Cursor
- Gemini CLI
- Visual Studio Code
- Antigravity

`studio-mcp` works with all of them, because every one of them starts an MCP server the same way: by running a program. The program is always the same, only the file you put it in changes.

## The command

Whatever your client, this is what you point it at:

```
/home/YOURNAME/.tuxblox/studio-mcp
```

Replace `YOURNAME` with your username. Use the full path, not `~`. Most MCP clients do not expand a tilde and will simply fail to start the server.

To get the exact path for your machine:

```bash
echo ~/.tuxblox/studio-mcp
```

## Claude Code

One command, from any directory:

```bash
claude mcp add roblox-studio -- ~/.tuxblox/studio-mcp
```

Add `--scope user` if you want it available in every project rather than just the current one:

```bash
claude mcp add roblox-studio --scope user -- ~/.tuxblox/studio-mcp
```

Check it registered:

```bash
claude mcp list
```

## Codex CLI

```bash
codex mcp add roblox-studio -- ~/.tuxblox/studio-mcp
```

Or write it into `~/.codex/config.toml` yourself:

```toml
[mcp_servers.roblox_studio]
command = "/home/YOURNAME/.tuxblox/studio-mcp"
```

Check it registered:

```bash
codex mcp list
```

## Other clients

For anything configured through a JSON file, such as Claude Desktop, Cursor or VS Code, the entry looks like this:

```json
{
  "mcpServers": {
    "roblox-studio": {
      "command": "/home/YOURNAME/.tuxblox/studio-mcp"
    }
  }
}
```

Roblox's [setup guide](https://create.roblox.com/docs/studio/mcp) is where to look for which file your client keeps that in. The entry itself is the same wherever you are running, only the `command` differs, and on Linux it is always `studio-mcp`.

Restart your AI client after editing any of these, so it picks up the new server.

## Using it

Open Studio, open a place, then ask your assistant to do something in it.

Studio and your AI client both need to be running at the same time. Starting the assistant with Studio closed will not work, since there is nothing for the server to talk to.

## What it does

Short version: it finds `StudioMCP.exe` inside the virtual drive and runs it through TuxBlox's compatibility layer, passing anything it is given straight through.

It looks in three places, in order:

1. Next to the Studio version you have selected in the Versions tab, so the assistant talks to the same Studio the Launch button starts.
2. Next to Roblox Studio, found by resolving Studio's own shortcut in the virtual drive.
3. The most recently updated version folder in the virtual drive that has one, as a last resort.

It also runs the layer in its immediate mode, meaning it does not wait for the virtual drive to empty out before returning. Without that it would sit there forever while Studio was open.

If **Verify Roblox Integrity** is turned on in Settings, `studio-mcp` checks the MCP server is signed by Roblox before running it, the same check TuxBlox applies to Studio itself.

All logging is off, deliberately. MCP clients talk over the same pipe the server prints to, so anything else written there would break the connection.

`studio-mcp` needs nothing installed on your system beyond the standard C library. It does not download anything and it does not need a network connection.

## If it does not work

**`Roblox's Studio MCP server is not installed`**

`studio-mcp` searched the virtual drive and came up empty. Either Roblox's Studio MCP server is not installed, or it was installed outside TuxBlox's virtual drive. Go back to [Roblox's setup guide](https://create.roblox.com/docs/studio/mcp) and run through it with Studio open under TuxBlox.

**`the compatibility layer is missing`**

TuxBlox has not finished installing. Open TuxBlox once and let it settle, then try again.

**Your client says the server exited immediately**

Check the path is absolute and points at `studio-mcp`, not the old `mcp.sh`:

```bash
ls -l ~/.tuxblox/studio-mcp
```

**Your client connects but nothing happens in Studio**

Studio has to be open with a place loaded. Also check the Studio side is actually enabled: the plugin has to be installed and you have to have granted it permission inside Studio. That is the step people skip.

**Your client hangs on startup**

Make sure you are not passing `studio-mcp` through a shell wrapper that keeps the pipe open. The command should be the program itself and nothing else.

## Running it by hand

You can start it directly to see what it says:

```bash
~/.tuxblox/studio-mcp
```

It will then sit there waiting for MCP messages on standard input, which is not useful on its own. Any error it prints before that point is the thing to act on.

`~/.tuxblox/studio-mcp --version` prints the TuxBlox version it was built from, and `--help` prints a short summary.
