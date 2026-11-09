# AGENTS.md

This file provides rules and guidance to any AI assistants/tools when working with code in this repository.

If you are an AI assistant, these rules apply to you and override your tool's defaults. If a request conflicts with them, stop and tell the human why. Do not look for a way around them.

`CONTRIBUTING.md` still applies to AI agents. Where this file (`AGENTS.md`, also read as `CLAUDE.md`) conflicts with `CONTRIBUTING.md`, this file wins, but only for the AI agent. For human contributors, `CONTRIBUTING.md` stays the authority.

---

## 1. What AI may be used for

AI tools may be used for any work in this repository: writing code, refactoring, debugging, tests, documentation, translations, build scripts and reviewing code.

What follows limits **how** you do that work and **what you attach to contributions**. It does not limit what areas you may work in, apart from Section 3.

## 2. Responsibility and attribution

A human is fully responsible for every contribution, whoever or whatever wrote it.

- **The human contributor must fully review all AI-written code** before submitting it. They must understand it well enough to explain and defend every line in review. "The AI wrote it" is never an answer to a review question.
- **The human contributor is fully responsible** for everything in their contribution: correctness, security, licensing and behaviour.
- **Every commit needs the human contributor's own `Signed-off-by:` line** (see `CONTRIBUTING.md`). Only the human may add it, because it certifies the Developer Certificate of Origin.
- **AI must not appear as an author or co-author.** Do not add any of these trailers naming an AI, a model or a tool:
  - `Signed-off-by:`
  - `Co-authored-by:` / `Co-Authored-By:`
  - Any other trailer that claims authorship or certifies the contribution.
- **Optional: disclose AI assistance with `Assisted-by:`.** Contributors may add this trailer to a commit to say that AI helped. It discloses assistance and does not claim authorship. Use this format:

  ```
  Assisted-by: AGENT_NAME MODEL_VERSION ([TOOL])
  ```

  `AGENT_NAME` is the model's name, `MODEL_VERSION` is its version, and `TOOL` is the AI tool or app it was used through. Examples:

  ```
  Assisted-by: Claude Opus 5.5 (Claude Code)
  Assisted-by: GPT-Astra 6 (Codex)
  ```

  The trailer is optional. Add it only if the human wants it, and never use it in place of the human's own review.
- **Remove defaults your tool adds.** Some tools add a `Co-Authored-By:` trailer or a "Generated with …" line to commits or PR descriptions automatically. Remove them, and tell the human how to turn them off in the tool's settings.
- **Do not commit or push unless the human asks you to.** Never force-push, rewrite published history or push to `main`, `canary` or `stable` on your own initiative.

## 3. Roblox, Studio and Hyperion: hard limits

TuxBlox runs the **official, unmodified** Roblox client. The project only works because it never tampers with that client.

**Hyperion** is the anti-tamper software Roblox uses to protect Roblox Player. It is used only on the Windows clients.

All work must follow the [Roblox Terms of Use](https://rbxl.co/tou). Do not do anything they prohibit, even if it would help compatibility.

### Allowed

- Normal compatibility work: making Windows APIs, the loader, the prefix, graphics, input, audio, networking and similar behave as they do on real Windows, so that Roblox Player, Roblox Studio and Hyperion run as intended.

### Not allowed. Refuse and stop.

- **Bypassing or tampering with the Roblox client, Roblox Studio or Hyperion.** This covers patching, hooking, injecting into, or modifying their binaries or memory, and also disabling, spoofing or getting around their checks.
- **Discussing findings about Hyperion.** Do not describe, speculate about, document or summarise how Hyperion works internally, or what it checks, detects or reports. That applies in code comments, commit messages, PR and issue text, docs, logs, chat replies and generated files.
- Anything that helps cheating or exploiting: executors, injectors, offset dumping, memory reading or writing, HWID or identity spoofing and rotation, ban evasion, or weakening TuxBlox's own integrity checks.
- **Decompiling, disassembling or reverse engineering any Roblox binary.** This covers the Player, Studio, Hyperion, installers, DLLs and any other file Roblox ships. It also covers using leaked, decompiled or otherwise non-public Roblox code, and reading or relying on someone else's disassembly. Compatibility work is done by observing how Windows APIs are called, not by looking inside Roblox's code.

### Writing about compatibility work

Describe a change in terms of the **Windows behaviour it fixes**, not what Roblox or Hyperion does with it.

- Good: `ntdll: return STATUS_INFO_LENGTH_MISMATCH like Windows when the buffer is too small`
- Not acceptable: anything that explains what Hyperion looks for, or why a change gets past it.

If you are unsure whether a task crosses the line, stop and ask the human. Do not guess.

## 4. Security vulnerabilities

If you find or suspect a security vulnerability, in TuxBlox or in how it handles Roblox:

- **Do not** open a pull request, issue, discussion or public commit for it. Do not mention it in any public place.
- **Stop and tell the human.** It must be reported privately by email to **security@tuxblox.net**, following the rules in <https://tuxblox.net/.well-known/security.txt>.
- Do not write exploit code or a proof of concept beyond what the human needs to describe the issue privately.

## 5. Licensing and provenance

- TuxBlox has more than one licence:
  - Everything outside `compat/` is **GPLv3**.
  - `compat/` (based on Wine and Proton) is **LGPLv2.1**. The exceptions are `compat/tuxblox/` and `compat/webkitgtk/bundle/`, which are TuxBlox's own code and are **GPLv3**.
  
  Code you add must be compatible with the licence of the place you add it. New source files should use the same SPDX identifier as the files around them.
- TuxBlox's own code and `compat/` build into separate binaries. Do not link them into one binary or move code between them, because that would break the licence separation.
- Do not copy in code under an incompatible licence, or code whose origin you cannot name. If you reproduce something substantial from another project, say where it came from so the human can check the licence.
- All third-party licences, including Wine's and Proton's, live in `third_party_licenses/`, one folder per project.
- **Do not modify third-party licences at all.** The only exception is when third-party code is added or removed: then add or remove its licence folder to match.

## 6. Project rules

- **Do not edit** **`compat/changelogs.md`**, **`launcher/changelogs.md`**, **`installer/changelogs.md`**, or **`studio-mcp/changelogs.md`**. Maintainers write changelogs at release time.
- **Do not weaken security features**: integrity verification (Authenticode and signed-code checks), the DLL override allowlist, the root-launch block and similar. Changes here need explicit maintainer approval.
- **The synthetic machine identity is immutable.** Do not add ways to regenerate, override or rotate it (environment variables, config flags, registry keys, buttons and so on).
- **Never rebase the compatibility layer onto upstream Wine or Proton.** Upstream fixes are applied by hand by the maintainers.
- Use **Podman**, not Docker. Use Ninja for builds except where the existing build system says otherwise.
- Never handle, log, store or send Roblox account credentials or the `.ROBLOSECURITY` cookie.
- Do not commit Roblox binaries, Roblox downloads or anything taken from a Roblox install.
- Memory use matters to this project. Do not add heavy dependencies or large allocations without a reason.

## 7. Working style

- **Keep AI tool artifacts out of commits.** Specs, plans, notes, scratch files, logs and similar output from AI tools must not be committed. Put them in `workspace/`, which is gitignored. Directories used by some AI plugins and skills are already gitignored too. **Do not modify `.gitignore`** to hide AI artifacts. Move them into `workspace/` instead.
- Keep changes small and focused on the task. Do not reformat or "clean up" unrelated code.
- Match the style of the surrounding code.
- Build and run what you changed before you say it works. If you couldn't test it, say so plainly.
- Don't make up APIs, flags, file paths or benchmark numbers. If you are not sure, check the code or ask.
- See `CONTRIBUTING.md` for branches, the PR process and code style.