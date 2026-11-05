# TuxBlox translations

The launcher, installer and Roblox bootstrap share the catalogs in
`translations/`. The build embeds them as C++ tables, so a shipped binary
needs no locale files beside it.

## Adding a string

Include `i18n.h` and wrap the English text at the point where it is rendered:

```cpp
gtk_label_set_text(pLabel, i18n::tr("Settings"));
```

For values, use `{0}`, `{1}`, ... and `trf`:

```cpp
setButtonLabel(pButton, i18n::trf("Stopped {0}", {std::to_string(count)}));
```

For a count, use `trn` with the singular and plural English forms. `{}` is
replaced with the number:

```cpp
i18n::trn("{} entry", "{} entries", count);
```

Use `trc("context", "source")` only when the same English word needs different
translations in different places. Add the context to the JSON catalogs.

Do not wrap paths, URLs, environment variable names, JSON examples, FastFlag
names, channel identifiers or diagnostics that only go to session logs.

## Catalogs

- `translations/en.json` is the source catalog. It is generated from the
  `tr`/`trf`/`trn`/`trc` calls with `--write-en`.
- `translations/pt_BR.json` and `translations/es.json` must contain exactly the
  same keys.

Each file has the shape:

```json
{
  "language": "pt_BR",
  "strings": {
    "Settings": "Configurações",
    "Stopped {0}": "Parou {0}"
  },
  "contexts": {}
}
```

Rules:

- Preserve every placeholder exactly (`{0}`, `{1}`, `{}`).
- Keep UTF-8 raw; no HTML entities.
- Product names stay: TuxBlox, Roblox, Roblox Player, Roblox Studio, FastFlags,
  Discord, GitHub, Vulkan, OpenGL, DirectX 11.
- PT-BR uses "você". Spanish uses neutral "tú" and "equipo" for computer,
  "controlador" for driver.

## Checking

```sh
python3 shared/i18n/tools/check-translations.py --write-en
python3 shared/i18n/tools/check-translations.py --check
```

`--write-en` adds source keys that are missing from `en.json`. `--check` fails
on missing or stale keys and on placeholder mismatches. It needs `xgettext`
(package `gettext`), which the build container installs.

## Runtime behavior

`tr()` returns the English source until an entry point calls one of the
`initialize*` functions. The launcher and installer call them before building
any UI. The saved `language` setting accepts `system`, `en`, `pt_BR` and `es`.
`TUXBLOX_LANGUAGE` overrides it for child processes and tests, and
`--language` overrides it for the installer.
