# Versions

The Versions tab is where you decide which build of Roblox you are actually running.

Most people never need it. It is there for when a new Roblox release breaks a plugin you rely on, when you need to reproduce a bug against a specific build, or when you want to test your game on an older client.

## What a version looks like

Roblox identifies each build with a hash, like `version-a1b2c3d4e5f6789a`. That is what you see everywhere in this tab.

Player and Studio have separate lists, even though they share a folder inside the virtual drive. The tab shows them under two headings.

## The install bar

One row of controls along the top:

| Control | What it does |
|---|---|
| **Player / Studio** | Which app you are installing for |
| **Channel** | Roblox's release channel, `live` unless you have a reason |
| **Version** | A specific `version-…` hash, or leave it blank for the latest |
| **Install** | Downloads and installs what the fields describe |
| **Previous** | Installs the build immediately before the current latest |

**Previous** is the button most people want. It is the "Roblox broke something today" button.

While an install runs you get a progress bar and a label telling you which stage it is at: resolving the version, fetching the package list, downloading, then extracting. Studio is made up of more than thirty packages, so downloading takes a while.

Only one install can run at a time.

## The version list

Each installed build gets a row showing its hash, the channel it came from, and the date it was installed.

| Button | What it does |
|---|---|
| **Set active** | Makes that build the one that launches from now on |
| **Active** | Greyed out label, this is already the active build |
| **Delete** | Removes that build from the virtual drive, with a confirm step |

Deleting the active build is allowed. The next launch simply installs Roblox again.

## The list reflects reality, not bookkeeping

TuxBlox works out what is installed by looking in the virtual drive, not by trusting a list it wrote earlier. If you delete a version folder by hand it disappears from the tab, and if something appears there that TuxBlox did not install, it shows up.

That means the tab cannot get stuck telling you something is installed when it is not.

## Roblox still updates itself

Pinning a version is not the same as freezing Roblox forever. Roblox updates itself when it feels the need to, the same way it does on Windows, and joining an experience with a newer client requirement will pull the newer client.

Think of this tab as choosing where to start from, not as a lock.

## When there is nothing listed

A fresh install shows **No versions installed** under both headings. That is normal. Press **Install & Launch** on the Home tab, or install a version here, and the list fills in.
