#!/usr/bin/env python3
# TuxBlox - Linux Compatibility Layer for the Roblox Engine
# Copyright (C) 2026 TuxBlox Developers
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

import os
import re
import sys

root = sys.argv[1]


def readFile(path):
    with open(os.path.join(root, path)) as f:
        return f.read()


def writeFile(path, text):
    with open(os.path.join(root, path), "w") as f:
        f.write(text)


def replaceOnce(text, old, new, what):
    if text.count(old) != 1:
        sys.exit("ERROR: libadwaita patch did not match (%s): the source layout changed" % what)
    return text.replace(old, new)


meson = readFile("src/meson.build")
meson = re.sub(r"appstream_dep = dependency\('appstream',.*?\n\)\n", "", meson, count=1, flags=re.S)
meson = replaceOnce(meson, "  appstream_dep,\n", "", "appstream_dep in the dependency list")
writeFile("src/meson.build", meson)

for kind in ("dialog", "window"):
    path = "src/adw-about-%s.c" % kind
    text = readFile(path)
    text = replaceOnce(text, "#include <appstream.h>\n", "", "%s include" % kind)
    text = re.sub(r"static gboolean\nget_release_for_version \(AsRelease.*?\n}\n\n", "", text, count=1, flags=re.S)
    retType, cast, typeMacro = ("AdwDialog", "ADW_DIALOG", "ADW_TYPE_ABOUT_DIALOG") if kind == "dialog" else ("GtkWidget", "GTK_WIDGET", "ADW_TYPE_ABOUT_WINDOW")
    pattern = r"(%s \*\nadw_about_%s_new_from_appdata \(.*?\)\n)\{.*?\n\}\n" % (retType, kind)
    stub = r"\1{\n  return %s (g_object_new (%s, NULL));\n}\n" % (cast, typeMacro)
    text, n = re.subn(pattern, stub, text, count=1, flags=re.S)
    if n != 1:
        sys.exit("ERROR: libadwaita patch did not match (%s new_from_appdata)" % kind)
    if re.search(r"\bas_[a-z_]+ \(|\bAs[A-Z]", text):
        sys.exit("ERROR: libadwaita patch left AppStream calls in %s" % path)
    writeFile(path, text)

wrap = os.path.join(root, "subprojects/appstream.wrap")
if os.path.exists(wrap):
    os.remove(wrap)
