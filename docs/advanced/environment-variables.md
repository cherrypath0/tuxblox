# Environment Variables

TuxBlox can be steered with environment variables. The launcher sets the important ones for you, and the **Environment variables** box in [Settings](../using-tuxblox/settings.md#environment-variables) is where you add your own.

You do not need anything on this page for TuxBlox to work. Reach for it when you are debugging, benchmarking, or working around a driver.

## How to set them

**Through the launcher**, which is the way that persists. Settings, then Environment variables, as a space separated list:

```
DXVK_HUD=fps TUXBLOX_LOG=1
```

Variables you set here are applied last, so they override anything TuxBlox chose for you, including the graphics card picker.

**For a single run from a terminal**, if you are calling the layer yourself:

```bash
TUXBLOX_LOG=1 ~/.tuxblox/TuxBloxLauncher --launch-studio
```

---

## TuxBlox variables

### TUXBLOX_PREFIX

Path to the virtual drive. The launcher sets this to `~/.tuxblox/runtime`.

Only needs setting by hand if you are running `compat/main` directly. It is the one variable the layer cannot work without.

### TUXBLOX_LOG

Turns on logging. Off by default, because logging costs real frame time.

| Value | Effect |
|---|---|
| unset or `0` | No logging |
| `1` | A useful default set of channels |
| anything else | That value is added to the default set |

The advanced form takes Wine debug channel syntax, so `TUXBLOX_LOG=+seh,+relay` adds those channels on top of the defaults.

Turning this on also raises the graphics layers' own logging to a level that reports warnings.

### TUXBLOX_LOG_DIR

Where `tuxblox.log` is written. The launcher sets it to `~/.tuxblox/logs`. Defaults to your home folder if unset.

### TUXBLOX_HAPTICS

Set to `0` to turn off controller vibration. Anything else, including leaving it unset, leaves it on.

This is what the **Enable Haptics** setting writes.

### TUXBLOX_WEBVIEW_GPU

`1` or `0`, for GPU acceleration in the panels that are web pages. This is what the **GPU acceleration for web pages** setting writes.

Unlike haptics, both values are sent explicitly, because the layer's own default differs from the launcher's.

### TUXBLOX_NO_FSYNC

Set to `1` to drop down a rung on the thread synchronisation ladder.

TuxBlox picks the best mechanism your kernel offers, preferring `ntsync`, then `fsync`, then falling back to the compatibility layer's own server. This variable forces it off the faster paths.

Only useful for diagnosing a hang. It will not make anything faster.

### TUXBLOX_USE_WINED3D

Set to `1` to render Direct3D through WineD3D, which translates to OpenGL, instead of DXVK, which translates to Vulkan.

DXVK is the default and is faster nearly everywhere. WineD3D is worth trying only on hardware with no usable Vulkan driver.

### TUXBLOX_NO_D3D11

Set to `1` to disable Direct3D 11 entirely, pushing Roblox onto an older rendering path.

A diagnostic, not a tuning knob.

### TUXBLOX_DISABLE_NVAPI

Set to `1` to turn off NVIDIA's NVAPI support inside the layer. Only relevant on NVIDIA hardware, and only worth trying if something NVIDIA specific is misbehaving.

### TUXBLOX_DLL_COPY

Overrides which libraries get copied into the virtual drive rather than linked. Internal, and changing it will probably break your install. Listed for completeness.

---

## Graphics variables

These are not TuxBlox's, they belong to the graphics stack underneath. They work because TuxBlox passes your environment straight through.

### Picking a card

The **Graphics card** setting writes these for you. Set them yourself only if the picker gets your machine wrong.

**NVIDIA:**

```
__NV_PRIME_RENDER_OFFLOAD=1 __VK_LAYER_NV_optimus=NVIDIA_only __GLX_VENDOR_LIBRARY_NAME=nvidia
```

**AMD and Intel:**

```
DRI_PRIME=pci-0000_03_00_0 MESA_VK_DEVICE_SELECT=1002:744c
```

### Overlays and measurement

| Variable | What it does |
|---|---|
| `DXVK_HUD=fps` | DXVK's own framerate overlay |
| `DXVK_HUD=full` | Everything DXVK can report |
| `MANGOHUD=1` | MangoHud overlay, if you have it installed |

> [!NOTE]
> Roblox renders through Vulkan directly rather than through Direct3D, so DXVK's overlay may show nothing during actual gameplay. It is still useful for Studio's interface and for the launcher's own panels.

### Wine variables

The compatibility layer is Wine underneath, so Wine's variables work, `WINEDEBUG` included. TuxBlox sets sensible values for the ones that matter and will not overwrite one you set yourself.

Be careful with `WINEDEBUG`. Verbose channels can slow Roblox to a crawl and produce gigabytes of log.

---

## Variables TuxBlox sets for itself

You will see these in logs. They are set by the layer at startup and are not meant to be set by hand.

| Variable | Purpose |
|---|---|
| `TUXBLOX_WEBVIEW_DIR` | Where the bundled browser component lives |
| `TUXBLOX_REAL_EXIT_CODE` | How Roblox's real exit code is reported back |
| `TUXBLOX_WRAPPER_EXIT_CODE` | The layer's own exit code |
| `TUXBLOX_VERSION`, `TUXBLOX_CHANNEL` | Baked in at build time |

---

## Variables that no longer exist

TuxBlox used to use Proton's `PROTON_*` and Steam's `STEAM_COMPAT_*` names. They were renamed to `TUXBLOX_*` and the ones that only applied to Steam games were removed.

If you are following an old guide, `STEAM_COMPAT_DATA_PATH` is now `TUXBLOX_PREFIX` and `PROTON_LOG` is now `TUXBLOX_LOG`.
