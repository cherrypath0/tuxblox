# TuxBlox translations

The launcher, the installer and the Roblox bootstrap share the catalogues in
`shared/translations/`. They are ordinary gettext `.po` files, so Poedit,
Weblate and the rest of the usual tooling work on them. The build turns them
into C++ tables, so a shipped binary needs no locale files beside it.

## Adding a string

Include `i18n.h` and wrap the English text where it is rendered:

```cpp
gtk_label_set_text(pLabel, i18n::tr("Settings"));
```

For values, use `{0}`, `{1}`, ... and `trf`:

```cpp
setButtonLabel(pButton, i18n::trf("Stopped {0}", {std::to_string(count)}));
```

For a count, use `trn` with the singular and plural English forms. `{}` is
replaced with the number, and each language picks its form by its own rule --
Brazilian Portuguese puts zero with the singular, English does not:

```cpp
i18n::trn("{} entry", "{} entries", count);
```

Use `trc("context", "source")` only when the same English word needs different
translations in different places.

Do not wrap paths, URLs, environment variable names, JSON examples, FastFlag
names, channel identifiers or diagnostics that only go to session logs.

After changing any user-facing text, run:

```sh
python3 shared/i18n/tools/update-translations.py
```

It re-extracts `shared/translations/tuxblox.pot` with `xgettext` and brings
every `.po` forward with `msgmerge`, then prints what each language still has
left. Commit what it writes. It needs the `gettext` package; the build itself
does not, and deliberately never writes into the tree.

## Catalogues

- `shared/translations/tuxblox.pot` is the extracted template. It is generated,
  never edited by hand.
- Every `shared/translations/*.po` is a language. The file name is the language
  tag that `settings.json` stores, so `pt_BR.po` is `"language": "pt_BR"`.
- The build and the checker discover them from the directory, so **adding a
  language is one file and no code**.

Each catalogue needs these headers:

```
"Language: pt_BR\n"
"Plural-Forms: nplurals=2; plural=(n > 1);\n"
"X-Native-Name: Português (Brasil)\n"
```

`X-Native-Name` is what the picker shows, written in the language itself so
somebody who cannot read the current one can still find theirs.
`Plural-Forms` decides which form `trn` picks and is used as written.

Rules:

- Preserve every placeholder exactly (`{0}`, `{1}`, `{}`).
- Keep UTF-8 raw; no HTML entities.
- Product names stay: TuxBlox, Roblox, Roblox Player, Roblox Studio, FastFlags,
  Discord, GitHub, Vulkan, OpenGL, DirectX 11.
- PT-BR uses "você". Spanish uses neutral "tú", "equipo" for computer and
  "controlador" for driver.
- An entry left empty, or marked `fuzzy`, renders as the English source. That
  is the intended way to leave a string for later -- it is never blank on
  screen.
- A catalogue with nothing translated in it is left out of the build entirely,
  so a language somebody has started does not appear in the picker as English.

## Checking

```sh
python3 shared/i18n/tools/check-translations.py --check
```

Fails when a catalogue has drifted from the template, and when a translation
drops or invents a placeholder. It runs as a test in both the launcher and the
installer builds and needs nothing but python3.

## Runtime behaviour

`tr()` returns the English source until an entry point calls one of the
`initialize*` functions. The launcher and the installer call them before
building any UI. The saved `language` setting is `system` or a tag some
catalogue carries; anything else reads as `system`. `TUXBLOX_LANGUAGE`
overrides it for child processes and tests, and `--language` overrides it for
the installer. The choice applies at the next start.
