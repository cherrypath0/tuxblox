# Frequently Asked Questions

## Is TuxBlox free?

Yes, and it always will be. TuxBlox is free and open source. There is nothing to buy, no account to make, and no paid tier.

## Does Roblox Studio work?

Yes. Studio is the part of TuxBlox that works today, and it is what most of the project's users run.

## Does Roblox Player work?

Not yet.

Roblox's anti-cheat blocks Roblox Player from starting under every Wine based compatibility layer, including TuxBlox. This is the project's largest ongoing piece of work and there is real progress, but there is no date.

Studio is not affected. See [Known Issues](../troubleshooting/known-issues.md) for where things stand.

## Will I get banned for using TuxBlox?

TuxBlox does not modify Roblox, inject anything into it, or give you any ability a Windows player does not have. It runs the official, unmodified client.

Roblox's CEO has publicly said compatibility layers are acceptable, though that was a spoken answer rather than written policy. We cannot make promises on Roblox's behalf, so use your own judgement.

What we can tell you is what TuxBlox deliberately refuses to do: anything that hides cheating, automates input, edits game memory, or grants a capability the Windows client does not have. None of that will ever be accepted into this project.

## Is TuxBlox affiliated with Roblox?

No. TuxBlox is an independent project with no affiliation with, endorsement from, or connection to Roblox Corporation.

## How is this different from Sober or Vinegar?

[Sober](https://sober.vinegarhq.org/) runs the Android version of Roblox on Linux. TuxBlox runs the Windows version. Different client, different tradeoffs.

The short comparison: Sober gets you playing games today, and TuxBlox gets you Studio and, eventually, the full desktop client.

## Do I need to install Wine or Proton first?

No. TuxBlox ships its own, and it does not use or interfere with any Wine or Proton you already have installed. Steam's Proton is untouched.

## Can I use the compatibility layer for other Windows programs?

You can try. It is offered as a separate download on the [releases page](https://tuxblox.net/releases).

It is built and tuned for Roblox specifically, though, with plenty of general purpose pieces removed. If something else does not work under it, that is expected rather than a bug. Use regular Wine or Proton for general Windows software.

## Where does TuxBlox install things?

Everything lives in `~/.tuxblox`, plus a handful of desktop entry files in the standard XDG locations so TuxBlox shows up in your applications menu. Nothing goes into system directories.

See [Files and Folders](../advanced/files-and-folders.md) for the full layout.

## Does it need root or sudo?

No. Not to install, not to run, not to update.

## Can I use FastFlags?

Yes, there is an editor built into the launcher. See [FastFlags](../using-tuxblox/fastflags.md).

## Can I run an old version of Roblox Studio?

Yes, from the [Versions](../using-tuxblox/versions.md) tab. You can install a specific build by its version hash, or step back to the build before the current one.

## Does my controller work?

Yes. Controller vibration is a separate, experimental setting, off-by-default behaviour that varies by pad. See [Settings](../using-tuxblox/settings.md#enable-haptics).

## Does Studio's Toolbox work? What about the Creator Hub pages?

Yes. Those panels are web pages rendered inside Studio, and TuxBlox ships its own browser component to render them.

## Can I use the Studio MCP server with an AI assistant?

Yes. TuxBlox includes a helper script for exactly that, and it works with every client Roblox supports: Claude Code, Claude Desktop, Codex CLI, Cursor, Gemini CLI, Visual Studio Code and Antigravity.

You still have to set up the Studio side first, following [Roblox's guide](https://create.roblox.com/docs/studio/mcp). See [Studio MCP](../using-tuxblox/studio-mcp.md) for the Linux specific part.

## Does TuxBlox collect data about me?

Only crash reports, and only if you leave that setting on. A crash report contains the exit code, your Roblox and TuxBlox versions, basic system information, and the session log.

You can turn it off in Settings at any time. The full policy is at [tuxblox.net/privacy](https://tuxblox.net/privacy).

## Where can I get help?

The [Discord server](https://tuxblox.net/discord). It is the fastest place for support, and it is where development is discussed.
