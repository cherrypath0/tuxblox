# Known Issues with TuxBlox

This page lists some known problems and limitations currently affecting TuxBlox.

If you have found any issue, please report them in our [Discord server](https://discord.gg/tfdR4jU4kp) so we can fix them.

***

### Roblox Player does not work

This is currently one of the most significant limitations of TuxBlox. Roblox stopped allowing Wine-based environments in 2023, which prevents Roblox Player from functioning normally under Wine.

We are actively working on a solution for this.

***

### Black boxes randomly appear when clicking a dropdown menu in Roblox Studio

This is an issue inherited from upstream Wine. It is related to how Wine interacts with Linux window managers when handling certain windows and menus.

A fix is currently being worked on. There is currently no known user-side workaround.

***

### RAM usage is higher than expected

TuxBlox and Wine handle memory differently from Windows, which can result in higher RAM usage in some situations. This can also affect Roblox's own memory usage.

We are working on improvements to reduce unnecessary memory usage.

***

### The login screen or Toolbox does not work properly

Roblox uses web-based components for parts of its interface, and these components can have compatibility issues when running through Wine.

To improve compatibility, TuxBlox includes an experimental WebKitGTK-based web runtime. This implementation is still experimental, so some web-based Roblox interfaces may not work correctly.

***

### Roblox Studio lags when resizing a window

This is another issue inherited from upstream Wine. It is related to how Wine handles window resizing and rendering.

There is currently no known user-side fix. We plan to address this through changes to TuxBlox's compatibility layer.

***
