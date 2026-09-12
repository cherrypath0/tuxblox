# FastFlags

FastFlags are Roblox's own internal settings. Roblox uses them to turn features on and off, run experiments, and tune the engine without shipping a new build. TuxBlox lets you set them yourself.

> [!WARNING]
> Since September 2025, Roblox only permits players to change flags on an **allowlist**, and **may take action against your account for using flags outside it**. This is Roblox's rule, not ours, and TuxBlox cannot protect you from it. If you do not already know what a flag does, the safest thing is to leave it alone.

## The allowlist

Roblox published the allowlist, and the announcement explaining it, on the Developer Forum:

**[Allowlist for Local Client Configuration via Fast Flags](https://devforum.roblox.com/t/allowlist-for-local-client-configuration-via-fast-flags/3966569)**

Read it before you set anything. A few things worth knowing:

- The list in that post is not always current. Roblox updates the allowlist itself more often than it updates the post.
- Anything not on the list is at your own risk, and the risk is your account.
- Old guides and flag lists from before September 2025 predate the allowlist entirely. Most of what they recommend is no longer allowed.

TuxBlox does not check your flags against the list. It sets whatever you type, exactly like the Windows client would.

## The three kinds of flag

You can tell what a flag expects from the prefix on its name.

| Prefix | Type | Example value |
|---|---|---|
| `FFlag`, `DFFlag`, `SFFlag` | On or off | `True` or `False` |
| `FInt`, `DFInt`, `SFInt` | A whole number | `120` |
| `FString`, `DFString`, `SFString` | Text | `https://example.com/` |

The extra letters have meanings inside Roblox (`D` for dynamic, `S` for synchronised) but for your purposes they behave the same way.

TuxBlox does not check the type for you. It passes whatever you type straight through, because Roblox accepts a text value for every flag type anyway.

## Using the editor

Open the **FastFlags** tab. At the top are two buttons, **Player** and **Studio**. Each keeps its own separate list, so a flag you set for Studio does not affect Player.

To add a flag:

1. Press **+ Add flag**.
2. Type the flag name in the left box, exactly as Roblox spells it. Names are case sensitive.
3. Type the value in the right box.

To remove one, press the **−** button at the end of its row.

Changes are saved as you type. There is no save button.

### Duplicates

If two rows set the same flag name, the row marked **duplicate** is telling you the last one wins. Nothing breaks, it is just that only one of them can take effect.

## When flags take effect

The next time you start Roblox. TuxBlox writes your flags into the active Roblox version's settings folder at every launch, so editing a flag while Roblox is open does nothing until you restart it.

Clearing every flag removes the file entirely, which puts Roblox back to completely stock behaviour.

## Where they are stored

Your flags live in `~/.tuxblox/settings.json`, under `fast_flags`, split into `player` and `studio` lists:

```json
{
  "fast_flags": {
    "player": [],
    "studio": [
      { "name": "FFlagDebugDisplayFPS", "value": "True" },
      { "name": "DFIntTaskSchedulerTargetFps", "value": "120" }
    ]
  }
}
```

At launch, TuxBlox turns the relevant list into a `ClientAppSettings.json` inside the running Roblox version's folder, which is where Roblox actually reads them from.

It is written at every launch rather than once, because Roblox updates put each build in its own folder and the file has to follow.

## Finding flag names

Start with [Roblox's allowlist post](https://devforum.roblox.com/t/allowlist-for-local-client-configuration-via-fast-flags/3966569). That is the only list with any authority behind it.

Community maintained lists exist and vary enormously in quality. TuxBlox does not ship a list and does not recommend any flags.

A few sanity rules:

- A flag that does not exist is ignored, so a typo is harmless but also silent.
- A flag that exists but expects a different type may make Roblox behave unpredictably.
- Flags disappear between Roblox releases. A list from a year ago is mostly fiction, and a list from before September 2025 is mostly not allowed either.
- If a "performance flag" sounds too good to be true, it is.

## Turning everything off again

Remove every row. The next launch deletes the override file and Roblox goes back to its own defaults.

If you have a FastFlag setup you are not sure about and Roblox is misbehaving, clearing the list is the first thing to try, before anything in [Troubleshooting](../troubleshooting/common-problems.md).
