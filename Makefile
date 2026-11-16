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

SHELL := /bin/bash
ROOT := $(CURDIR)
JOBS ?= $(shell nproc)

-include config.mk

ifeq ($(origin SOURCE_DATE_EPOCH),undefined)
SOURCE_DATE_EPOCH := $(shell git -C $(ROOT) log -1 --format=%ct 2>/dev/null)
endif

SOURCE_DATE_EPOCH := $(or $(strip $(SOURCE_DATE_EPOCH)),$(shell date +%s))
export SOURCE_DATE_EPOCH

TUXBLOX_RM_DIST ?=

NODIST ?=

BUILD := $(ROOT)/build
ART := $(BUILD)/.artifacts
PROTON := $(ART)/proton
UI_DIST := $(ROOT)/shared/ui/dist

NATIVE := $(BUILD)/native
LAUNCHER := $(NATIVE)/TuxBloxLauncher
INSTALLER := $(NATIVE)/TuxBloxInstaller
STUDIO_MCP := $(NATIVE)/studio-mcp
COMPAT := $(NATIVE)/compat/main
UI_STACK_VERSION := $(shell sed -n 's/^UI_STACK_VERSION=//p' shared/ui/versions.env)
UI := $(UI_DIST)/ui-stack-$(UI_STACK_VERSION)-x86_64.tar.zst
LIBS := $(ART)/shared/lib/libcurl.a
PATCHES := $(ART)/.patches
PODMAN := $(ART)/.podman

COMPAT_VERSION := $(shell cat compat/VERSION 2>/dev/null)

RELEASE_AWK = awk 'NR==1{s=$$0} NR==2{b=$$0} END{ gsub(/[ \t\r]/,"",s); gsub(/[ \t\r]/,"",b); n=split(s,v,"."); if (n==4) print s; else if (n==3) { if (b !~ /^[0-9]+$$/) b="0"; print v[1]"."v[2]"."b"."v[3] } }' VERSION
RELEASE = $(shell $(RELEASE_AWK) 2>/dev/null)
CHANNEL = $(shell sed -n 3p VERSION 2>/dev/null | tr -d '[:space:]')

LIBADWAITA_DIR := $(shell cd $(ROOT)/shared/ui 2>/dev/null && pwd -P)/libadwaita
LIBADWAITA_HEAD = $(shell d=$(LIBADWAITA_DIR); [ "$$(git -C $$d rev-parse --show-toplevel 2>/dev/null)" = "$$d" ] || exit 0; printf '%s%s' "$$(git -C $$d rev-parse HEAD)" "$$([ -n "$$(git -C $$d status --porcelain)" ] && echo -dirty)")
LIBADWAITA_SHORT = $(shell d=$(LIBADWAITA_DIR); [ "$$(git -C $$d rev-parse --show-toplevel 2>/dev/null)" = "$$d" ] || exit 0; printf '%s%s' "$$(git -C $$d rev-parse --short HEAD)" "$$([ -n "$$(git -C $$d status --porcelain)" ] && echo -dirty)")
LIBADWAITA_COMMIT = $(shell for t in $(UI_DIST)/ui-stack-*.tar.zst; do [ -f "$$t" ] || continue; c=$$(zstd -dc "$$t" 2>/dev/null | tar -xO LIBADWAITA_COMMIT 2>/dev/null | tr -d '[:space:]'); if [ -n "$$c" ]; then printf '%s' "$$c"; exit 0; fi; done; printf '%s' "$(LIBADWAITA_HEAD)")
CCACHE_DIR ?= $(HOME)/.ccache

UI_MOUNTS := -v $(CURDIR)/shared/ui/src:/ui-src:ro -v $(CURDIR)/shared/ui/dist/dev:/ui-dev:ro
UI_ARGS := -DTUXBLOX_UI_STACK_DEV=/ui-dev -DTUXBLOX_UI_SRC=/ui-src
ifdef TUXBLOX_HEADLESS_ONLY
UI_MOUNTS :=
UI_ARGS :=
endif
WEBKITGTK_VERSION := $(shell sed -n 's/^WEBKITGTK_VERSION=//p' compat/webkitgtk/bundle/versions.env 2>/dev/null)
WEBVIEW := $(ROOT)/compat/webkitgtk/prebuilt/webkitgtk-$(WEBKITGTK_VERSION)-x86_64.tar.xz

FIND := find -L
SHARED_SRC := $(shell $(FIND) shared -type f \
	-not -path 'shared/ui/dist/*' -not -path 'shared/ui/sources/*' \
	-not -path 'shared/ui/libadwaita/*' 2>/dev/null)
LAUNCHER_SRC := $(shell $(FIND) launcher/src launcher/CMakeLists.txt -type f 2>/dev/null)
INSTALLER_SRC := $(shell $(FIND) installer/src installer/CMakeLists.txt -type f 2>/dev/null)
STUDIO_MCP_SRC := $(shell $(FIND) studio-mcp/src studio-mcp/CMakeLists.txt -type f 2>/dev/null)
COMPAT_SRC := $(shell $(FIND) compat/tuxblox compat/wine/dlls compat/wine/programs compat/patches \
	compat/Makefile.in compat/configure.sh compat/webkitgtk/prebuilt -type f 2>/dev/null)

export TUXBLOX_SKIP_DEPS := 1
export TUXBLOX_MAKE_JOBS := $(JOBS)
export TUXBLOX_COMPAT_VERSION := $(COMPAT_VERSION)
export TUXBLOX_RELEASE = $(RELEASE)
export TUXBLOX_CHANNEL = $(CHANNEL)

.NOTPARALLEL:

.PHONY: all
all: bump-build record-provenance components check copy-include packages

.PHONY: packages
ifdef TUXBLOX_NO_PACKAGES
packages:
	@printf ':: TUXBLOX_NO_PACKAGES set, the AppImage and the Flatpak were NOT built\n'
else
packages: appimage flatpak
endif

.PHONY: components
components: bump-build shared-state launcher installer studio-mcp compat

.PHONY: shared-state
shared-state:
	@mkdir -p $(BUILD)/runtime $(BUILD)/roblox $(BUILD)/installers $(NATIVE) $(FLATPAK_DIR)
	@for d in runtime roblox installers; do \
	    ln -sfn ../$$d $(NATIVE)/$$d; \
	    ln -sfn ../$$d $(FLATPAK_DIR)/$$d; \
	done

.PHONY: launcher installer studio-mcp compat deps appimage flatpak assets
launcher: bump-build record-provenance $(LAUNCHER)
installer: bump-build record-provenance $(INSTALLER)

ASSETS_DIR := $(BUILD)/assets
ASSET_SIZES := 1024 512 256 128 64 32
ASSET_SVGS := $(wildcard assets/*.svg)
TUXBLOX_ICONS := $(foreach s,$(ASSET_SIZES),$(ASSETS_DIR)/tuxblox/$(s)x$(s).png)

assets: $(TUXBLOX_ICONS)

$(TUXBLOX_ICONS): $(ASSETS_DIR)/tuxblox/%.png: assets/tuxblox.svg
	@command -v rsvg-convert >/dev/null 2>&1 || { \
	    printf '!! rsvg-convert not found, so the icons cannot be rendered. Install librsvg.\n' >&2; exit 1; }
	@mkdir -p $(@D)
	rsvg-convert -w $(firstword $(subst x, ,$*)) -h $(firstword $(subst x, ,$*)) $< -o $@

APPIMAGE_DIR := $(BUILD)/appimage
APPIMAGE_APPDIR := $(APPIMAGE_DIR)/TuxBloxInstaller.AppDir
APPIMAGE := $(APPIMAGE_DIR)/TuxBloxInstaller.AppImage

appimage:
	@mkdir -p $(APPIMAGE_DIR)
	$(MAKE) --no-print-directory TUXBLOX_APPIMAGE=1 TUXBLOX_SKIP_SMOKE=1 INSTALLER=$(APPIMAGE_DIR)/TuxBloxInstaller installer
	@rm -rf $(APPIMAGE_APPDIR)
	@mkdir -p $(APPIMAGE_APPDIR)/usr/bin $(APPIMAGE_APPDIR)/usr/share/licenses
	tar --zstd -xf $(UI) -C $(APPIMAGE_APPDIR)/usr
	cp $(ART)/installer/TuxBloxInstaller-ui $(APPIMAGE_APPDIR)/usr/TuxBloxInstaller-ui
	cp $(APPIMAGE_DIR)/TuxBloxInstaller $(APPIMAGE_APPDIR)/usr/bin/TuxBloxInstaller
	cp $(ASSETS_DIR)/tuxblox/256x256.png $(APPIMAGE_APPDIR)/tuxblox-installer.png
	cp packaging/appimage/tuxblox-installer.desktop $(APPIMAGE_APPDIR)/tuxblox-installer.desktop
	cp -a third_party_licenses/. $(APPIMAGE_APPDIR)/usr/share/licenses/
	ln -sf usr/bin/TuxBloxInstaller $(APPIMAGE_APPDIR)/AppRun
	@if command -v appimagetool >/dev/null 2>&1; then \
	    appimagetool $(APPIMAGE_APPDIR) $(APPIMAGE); \
	else \
	    printf '!! appimagetool was not found, so the AppDir was assembled but not packed\n' >&2; \
	    printf '   it runs in place: APPDIR=%s %s/AppRun\n' '$(APPIMAGE_APPDIR)' '$(APPIMAGE_APPDIR)' >&2; \
	fi
	@printf ':: the AppImage variant stays in %s, build/native/TuxBloxInstaller is untouched\n' '$(APPIMAGE_DIR)'

FLATPAK_DIR := $(BUILD)/flatpak
FLATPAK_BUNDLE := $(FLATPAK_DIR)/net.tuxblox.TuxBlox.flatpak
FLATPAK_MANIFEST := packaging/flatpak/net.tuxblox.TuxBlox.yml

flatpak:
	@[ -e $(STUDIO_MCP) ] || { printf '!! not built yet: %s\n   run `make` first, then `make flatpak`\n' '$(STUDIO_MCP)' >&2; exit 1; }
	@mkdir -p $(FLATPAK_DIR)
	$(MAKE) --no-print-directory TUXBLOX_DONT_INCREMENT_BUILD=1 TUXBLOX_FLATPAK=1 \
	    LAUNCHER=$(FLATPAK_DIR)/TuxBloxLauncher NATIVE=$(FLATPAK_DIR) launcher
	@sed -e 's/@RELEASE@/$(RELEASE)/' \
	     -e "s/@DATE@/$$(git log -1 --format=%cd --date=short 2>/dev/null || echo 1970-01-01)/" \
	     -e 's|<description>@NOTES@</description>||' \
	    packaging/flatpak/net.tuxblox.TuxBlox.metainfo.xml.in \
	    > $(FLATPAK_DIR)/net.tuxblox.TuxBlox.metainfo.xml
	@rm -rf $(FLATPAK_DIR)/licenses
	@cp -rL third_party_licenses $(FLATPAK_DIR)/licenses
	@if command -v flatpak-builder >/dev/null 2>&1; then FB=flatpak-builder; \
	elif flatpak info org.flatpak.Builder >/dev/null 2>&1; then FB="flatpak run org.flatpak.Builder"; \
	else printf '!! no flatpak-builder: install it, or `flatpak install flathub org.flatpak.Builder`\n' >&2; exit 1; fi; \
	$$FB --force-clean --default-branch=$(CHANNEL) \
	     --state-dir=$(FLATPAK_DIR)/.flatpak-builder \
	     --repo=$(FLATPAK_DIR)/repo $(FLATPAK_DIR)/build $(FLATPAK_MANIFEST)
	flatpak build-bundle \
	    --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
	    $(FLATPAK_DIR)/repo $(FLATPAK_BUNDLE) net.tuxblox.TuxBlox $(CHANNEL)
	@printf ':: install it with: flatpak install --user %s\n' '$(FLATPAK_BUNDLE)'
	@printf ':: the Flatpak variant stays in %s, build/native/TuxBloxLauncher is untouched\n' '$(FLATPAK_DIR)'
studio-mcp: bump-build record-provenance $(STUDIO_MCP)
ifdef NODIST
compat: bump-build record-provenance compat-build
else
compat: bump-build record-provenance $(COMPAT)
endif

.PHONY: bump-build
bump-build:
	@if [ -n "$$TUXBLOX_DONT_INCREMENT_BUILD" ]; then \
	    printf ':: Build counter held at %s by TUXBLOX_DONT_INCREMENT_BUILD\n' \
	        "$$(sed -n 2p VERSION | tr -d '[:space:]')"; \
	else \
	    Counter="$$(sed -n 2p VERSION | tr -d '[:space:]')"; \
	    case "$$Counter" in '' | *[!0-9]*) \
	        printf '!! line 2 of VERSION is the build counter and reads "%s"\n' "$$Counter" >&2; \
	        exit 1 ;; \
	    esac; \
	    Tmp="$$(mktemp)"; \
	    awk -v n="$$((Counter + 1))" 'NR==2 { print n; next } { print }' VERSION > "$$Tmp" \
	        && cat "$$Tmp" > VERSION && rm -f "$$Tmp"; \
	fi
	@printf ':: Building release %s on %s\n' \
	    "$$($(RELEASE_AWK))" "$$(sed -n 3p VERSION | tr -d '[:space:]')"


.PHONY: record-provenance
record-provenance:
	@mkdir -p $(BUILD)
	@# VERSION is excluded because bump-build writes it, so no build would ever read as clean.
	@Commit="$$(git -C $(ROOT) rev-parse HEAD 2>/dev/null)"; \
	 Dirty=true; \
	 if [ -n "$$Commit" ] && \
	    [ -z "$$(git -C $(ROOT) status --porcelain -- ':(exclude)VERSION' 2>/dev/null)" ]; then \
	     Dirty=false; \
	 fi; \
	 printf '%s\n%s\n\n' "$$Commit" "$$Dirty" > $(BUILD)/.provenance; \
	 if [ -z "$$Commit" ]; then \
	     printf '!! not a git checkout, so this build cannot say what source it came from\n' >&2; \
	 elif [ "$$Dirty" = true ]; then \
	     printf ':: Built from %s with uncommitted changes, which a release cannot publish\n' \
	         "$$(echo "$$Commit" | cut -c1-9)" >&2; \
	 else \
	     printf ':: Built from %s\n' "$$(echo "$$Commit" | cut -c1-9)"; \
	 fi

$(PODMAN): Containerfile
	@mkdir -p $(dir $@)
	podman info >/dev/null
	podman build -t tuxblox-builder -f $(ROOT)/Containerfile $(ROOT)
	@touch $@

$(PATCHES): compat/Makefile.in $(wildcard compat/patches/*/*)
	@mkdir -p $(dir $@)
	$(ROOT)/compat/apply-patches.sh
	@touch $@

LIBS_SUBMODULES := shared/crypto/openssl:OPENSSL_COMMIT shared/network/libcurl:CURL_COMMIT \
    shared/network/nghttp2:NGHTTP2_COMMIT shared/fs/libarchive:LIBARCHIVE_COMMIT \
    shared/fs/zstd:ZSTD_COMMIT shared/fs/zlib:ZLIB_COMMIT
LIBS_COMMIT_ARGS = $(foreach e,$(LIBS_SUBMODULES),-e $(lastword $(subst :, ,$(e)))=$(shell git -C $(ROOT)/$(firstword $(subst :, ,$(e))) rev-parse --short HEAD 2>/dev/null))

$(LIBS): $(wildcard shared/tools/libs-*.sh) | $(PODMAN)
	@for e in $(LIBS_SUBMODULES); do p=$${e%%:*}; \
	    [ -e $(ROOT)/$$p/.git ] || { printf '!! %s is empty. Run: git submodule update --init %s\n' "$$p" "$$p" >&2; exit 1; }; \
	done
	@mkdir -p $(ART)/shared $(ART)/shared-build $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" $(LIBS_COMMIT_ARGS) \
	    -v $(ROOT):/src:ro,z -v $(ART)/shared:/out:z -v $(ART)/shared-build:/build:z \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G \
	    tuxblox-builder bash /src/shared/tools/libs-build-in-container.sh

define UI_BUILD
set -euo pipefail

source /src/ui/versions.env

Prefix=/opt/tuxblox-ui
LibDir="$$Prefix/lib/x86_64-linux-gnu"
Markers="$$Prefix/.done"
JOBS="$${JOBS:-$$(nproc)}"
SourcesDir="$${TUXBLOX_SOURCES_DIR:-/sources}"
mkdir -p "$$Prefix" "$$Markers" /build

export PKG_CONFIG_PATH="$$LibDir/pkgconfig:$$Prefix/lib/pkgconfig:$$Prefix/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$$LibDir -L$$LibDir"
export CPPFLAGS="-I$$Prefix/include"
export CFLAGS="-O2"
export CXXFLAGS="-O2"
export PATH="$$Prefix/bin:$$PATH"

extractSource() {
    local archive="$$1" destDir="$$2"
    local path="$$SourcesDir/$$archive"
    if [ ! -f "$$path" ]; then
        printf 'ERROR: missing source archive: %s\n' "$$archive" >&2
        printf '       run shared/ui/fetch-sources.sh to fill %s\n' "$$SourcesDir" >&2
        return 1
    fi
    rm -rf "$$destDir"
    mkdir -p "$$destDir"
    tar -xf "$$path" -C "$$destDir" --strip-components=1
}

mesonBuild() {
    local name="$$1"
    shift
    meson setup "/build/$$name/_b" "/build/$$name" --prefix="$$Prefix" "$$@"
    ninja -C "/build/$$name/_b" -j"$$JOBS" install
}

runStep() {
    local name="$$1" version="$$2" func="$$3" marker="$$Markers/$$1-$$2-stack$$UI_STACK_VERSION"
    if [ -e "$$marker" ]; then
        printf ':: Skipping %s %s (already built)\n' "$$name" "$$version"
        return 0
    fi
    printf ':: Building %s %s\n' "$$name" "$$version"
    "$$func" "$$name"
    touch "$$marker"
}

buildZlib() {
    extractSource "zlib-$${ZLIB_VERSION}.tar.gz" /build/zlib
    cd /build/zlib
    ./configure --prefix="$$Prefix" --libdir="$$LibDir"
    make -j"$$JOBS"
    make install
    rm -f "$$LibDir/libz.a"
    cd /build
}

buildPcre2() {
    extractSource "pcre2-$${PCRE2_VERSION}.tar.bz2" /build/pcre2
    cd /build/pcre2
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static \
        --enable-pcre2-8 --disable-pcre2-16 --disable-pcre2-32 --enable-unicode
    make -j"$$JOBS"
    make install
    cd /build
}

buildExpat() {
    extractSource "expat-$${EXPAT_VERSION}.tar.xz" /build/expat
    cd /build/expat
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static --without-docbook --without-examples --without-tests
    make -j"$$JOBS"
    make install
    cd /build
}

buildLibpng() {
    extractSource "libpng-$${LIBPNG_VERSION}.tar.xz" /build/libpng
    cd /build/libpng
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static
    make -j"$$JOBS"
    make install
    rm -f "$$LibDir/libpng.a"
    cd /build
}

buildLibffi() {
    extractSource "libffi-$${LIBFFI_VERSION}.tar.gz" /build/libffi
    cd /build/libffi
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static --disable-docs
    make -j"$$JOBS"
    make install
    mv "$$Prefix"/lib/lib/libffi* "$$LibDir/"
    rmdir "$$Prefix/lib/lib"
    sed -i 's|^toolexeclibdir=.*|toolexeclibdir=$${libdir}|' "$$LibDir/pkgconfig/libffi.pc"
    cd /build
}

buildGlib() {
    extractSource "glib-$${GLIB_VERSION}.tar.xz" /build/glib
    mesonBuild glib -Dtests=false -Dselinux=disabled -Dlibmount=disabled
}

buildZstd() {
    extractSource "zstd-$${ZSTD_VERSION}.tar.gz" /build/zstd
    make -C /build/zstd/lib -j"$$JOBS" install PREFIX="$$Prefix" libdir="$$LibDir"
    make -C /build/zstd/programs -j"$$JOBS" install PREFIX="$$Prefix"
}

buildLibtiff() {
    extractSource "tiff-$${TIFF_VERSION}.tar.gz" /build/libtiff
    cd /build/libtiff
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static --disable-jpeg --disable-old-jpeg --disable-jbig \
        --disable-lerc --disable-lzma --disable-zstd --disable-webp --disable-libdeflate --disable-cxx \
        --disable-tools --disable-tests --disable-contrib --disable-docs
    make -j"$$JOBS"
    make install
    cd /build
}

buildLibjpeg() {
    extractSource "libjpeg-turbo-$${JPEG_TURBO_VERSION}.tar.gz" /build/libjpeg
    cmake -S /build/libjpeg -B /build/libjpeg/_b -G Ninja -DCMAKE_INSTALL_PREFIX="$$Prefix" -DCMAKE_INSTALL_LIBDIR=lib/x86_64-linux-gnu \
        -DCMAKE_BUILD_TYPE=Release -DENABLE_STATIC=OFF -DWITH_TURBOJPEG=OFF -DWITH_JAVA=OFF
    ninja -C /build/libjpeg/_b -j"$$JOBS" install
}

buildPixman() {
    extractSource "pixman-$${PIXMAN_VERSION}.tar.gz" /build/pixman
    mesonBuild pixman -Dtests=disabled -Ddemos=disabled -Dgtk=disabled -Dlibpng=disabled
}

buildFribidi() {
    extractSource "fribidi-$${FRIBIDI_VERSION}.tar.xz" /build/fribidi
    mesonBuild fribidi -Ddocs=false -Dbin=false -Dtests=false
}

buildFreetype() {
    local harfbuzz="disabled"
    [ "$$1" = "freetype-pass2" ] && harfbuzz="enabled"
    if [ ! -e /build/freetype/meson.build ]; then
        extractSource "freetype-$${FREETYPE_VERSION}.tar.xz" /build/freetype
    fi
    rm -rf /build/freetype/_b
    mesonBuild freetype -Dharfbuzz="$$harfbuzz" -Dbrotli=disabled -Dpng=enabled -Dzlib=system -Dbzip2=disabled
}

buildCairo() {
    extractSource "cairo-$${CAIRO_VERSION}.tar.xz" /build/cairo
    mesonBuild cairo -Dtests=disabled
}

buildHarfbuzz() {
    extractSource "harfbuzz-$${HARFBUZZ_VERSION}.tar.xz" /build/harfbuzz
    mesonBuild harfbuzz -Dicu=disabled -Dutilities=disabled -Dtests=disabled -Ddocs=disabled -Dintrospection=disabled
}

buildPango() {
    extractSource "pango-$${PANGO_VERSION}.tar.xz" /build/pango
    mesonBuild pango -Dintrospection=disabled -Dbuild-testsuite=false -Dbuild-examples=false -Dlibthai=disabled -Dxft=disabled
}

buildGdkPixbuf() {
    extractSource "gdk-pixbuf-$${GDK_PIXBUF_VERSION}.tar.xz" /build/gdkpixbuf
    mesonBuild gdkpixbuf -Dbuiltin_loaders=png,jpeg -Dtiff=disabled -Dgif=disabled \
        -Dtests=false -Dman=false -Dintrospection=disabled -Dinstalled_tests=false
}

buildGraphene() {
    extractSource "graphene-$${GRAPHENE_VERSION}.tar.gz" /build/graphene
    mesonBuild graphene -Dtests=false -Dinstalled_tests=false -Dintrospection=disabled -Dgtk_doc=false
}

buildLibepoxy() {
    extractSource "libepoxy-$${EPOXY_VERSION}.tar.gz" /build/libepoxy
    mesonBuild libepoxy -Dtests=false -Ddocs=false
}

buildLibxkbcommon() {
    extractSource "libxkbcommon-$${XKBCOMMON_VERSION}.tar.xz" /build/libxkbcommon
    mesonBuild libxkbcommon -Denable-docs=false -Denable-tools=false -Denable-x11=false -Denable-wayland=false
}

buildWayland() {
    extractSource "wayland-$${WAYLAND_VERSION}.tar.xz" /build/wayland
    mesonBuild wayland -Ddocumentation=false -Dtests=false -Ddtd_validation=false
}

buildWaylandprotocols() {
    extractSource "wayland-protocols-$${WAYLAND_PROTOCOLS_VERSION}.tar.xz" /build/wayland-protocols
    mesonBuild wayland-protocols -Dtests=false
}

buildLibxml2() {
    extractSource "libxml2-$${LIBXML2_VERSION}.tar.xz" /build/libxml2
    cd /build/libxml2
    ./configure --prefix="$$Prefix" --libdir="$$LibDir" --disable-static --without-python --without-lzma --without-zlib --without-http --without-modules --without-readline
    make -j"$$JOBS"
    make install
    cd /build
}
buildGtk4() {
    extractSource "gtk-$${GTK4_VERSION}.tar.xz" /build/gtk4
    mesonBuild gtk4 -Dx11-backend=true -Dwayland-backend=true -Dvulkan=disabled \
        -Dmedia-gstreamer=disabled -Dbuild-tests=false -Dbuild-demos=false \
        -Dbuild-examples=false -Dbuild-testsuite=false -Dintrospection=disabled \
        -Ddocumentation=false -Dprint-cups=disabled -Dcloudproviders=disabled \
        -Dsysprof=disabled -Dcolord=disabled
}

buildLibadwaita() {
    rm -rf /build/libadwaita
    cp -a /src/ui/libadwaita /build/libadwaita
    rm -rf /build/libadwaita/.git
    mesonBuild libadwaita -Dintrospection=disabled -Dvapi=false -Dtests=false -Dexamples=false
}

buildAdwaitaFonts() {
    extractSource "adwaita-fonts-$${ADWAITA_FONTS_VERSION}.tar.xz" /build/adwaita-fonts
    mkdir -p "$$Prefix/fonts"
    find /build/adwaita-fonts -name 'AdwaitaSans-*.ttf' -exec cp -f {} "$$Prefix/fonts/" \;
    if ! ls "$$Prefix"/fonts/AdwaitaSans-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: no AdwaitaSans fonts in the adwaita-fonts archive\n' >&2
        exit 1
    fi
    find /build/adwaita-fonts -name 'AdwaitaMono-*.ttf' -exec cp -f {} "$$Prefix/fonts/" \;
    if ! ls "$$Prefix"/fonts/AdwaitaMono-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: no AdwaitaMono fonts in the adwaita-fonts archive\n' >&2
        exit 1
    fi
}

runStep zlib "$$ZLIB_VERSION" buildZlib
runStep pcre2 "$$PCRE2_VERSION" buildPcre2
runStep expat "$$EXPAT_VERSION" buildExpat
runStep libpng "$$LIBPNG_VERSION" buildLibpng
runStep libffi "$$LIBFFI_VERSION" buildLibffi
runStep glib "$$GLIB_VERSION" buildGlib
runStep zstd "$$ZSTD_VERSION" buildZstd
runStep libtiff "$$TIFF_VERSION" buildLibtiff
runStep libjpeg "$$JPEG_TURBO_VERSION" buildLibjpeg
runStep pixman "$$PIXMAN_VERSION" buildPixman
runStep fribidi "$$FRIBIDI_VERSION" buildFribidi
runStep freetype-pass1 "$$FREETYPE_VERSION" buildFreetype
runStep harfbuzz "$$HARFBUZZ_VERSION" buildHarfbuzz
runStep freetype-pass2 "$$FREETYPE_VERSION" buildFreetype
runStep cairo "$$CAIRO_VERSION" buildCairo
runStep pango "$$PANGO_VERSION" buildPango
runStep gdk-pixbuf "$$GDK_PIXBUF_VERSION" buildGdkPixbuf
runStep graphene "$$GRAPHENE_VERSION" buildGraphene
runStep libepoxy "$$EPOXY_VERSION" buildLibepoxy
runStep libxkbcommon "$$XKBCOMMON_VERSION" buildLibxkbcommon
runStep wayland "$$WAYLAND_VERSION" buildWayland
runStep wayland-protocols "$$WAYLAND_PROTOCOLS_VERSION" buildWaylandprotocols
runStep libxml2 "$$LIBXML2_VERSION" buildLibxml2
runStep gtk4 "$$GTK4_VERSION" buildGtk4
case "$${LIBADWAITA_SOURCE:?the Makefile passes the submodule commit}" in
    *-dirty) rm -f "$$Markers"/libadwaita-* ;;
    *) [ -e "$$Markers/libadwaita-$$LIBADWAITA_SOURCE" ] || rm -f "$$Markers"/libadwaita-* ;;
esac
runStep libadwaita "$$LIBADWAITA_SOURCE" buildLibadwaita
runStep adwaita-fonts "$$ADWAITA_FONTS_VERSION" buildAdwaitaFonts

printf ':: Assembling the icon theme\n'
themeDir="$$Prefix/share/icons/TuxBlox"
for iconSource in /build/gtk4/gtk/icons /build/libadwaita/src/icons; do
    if [ ! -d "$$iconSource" ]; then
        printf 'ERROR: %s is missing, so the icon theme cannot be assembled; delete %s and build again\n' "$$iconSource" "$$Markers" >&2
        exit 1
    fi
done
rm -rf "$$themeDir"
mkdir -p "$$themeDir"
cp -a /build/gtk4/gtk/icons/. "$$themeDir/"
cp -a /build/libadwaita/src/icons/. "$$themeDir/"
rm -f "$$themeDir/hicolor.index.theme"

printf ':: Rasterising the icons that exist only as SVG\n'
rasterised=0
while IFS= read -r svg; do
    name="$$(basename "$$svg" .svg)"
    if find "$$themeDir" -name "$$name.png" -o -name "$$name.symbolic.png" | grep -q .; then
        continue
    fi
    category="$$(basename "$$(dirname "$$svg")")"
    case "$$name" in
        *-symbolic | *-symbolic-rtl | *-symbolic-ltr) twin="$$themeDir/16x16/$$category/$$name.symbolic.png" ;;
        *) twin="$$themeDir/16x16/$$category/$$name.png" ;;
    esac
    mkdir -p "$$(dirname "$$twin")"
    if ! rsvg-convert -w 16 -h 16 -o "$$twin" "$$svg" || [ ! -s "$$twin" ]; then
        printf 'ERROR: %s could not be rendered, so that icon would draw as a missing image\n' "$$svg" >&2
        exit 1
    fi
    rasterised=$$((rasterised + 1))
done < <(find "$$themeDir/scalable" -name '*.svg' | sort)
printf ':: %s icons rendered\n' "$$rasterised"

iconDirs="$$(cd "$$themeDir" && find . -mindepth 2 -type d | sed 's#^\./##' | sort)"
{
    printf '[Icon Theme]\nName=TuxBlox\nComment=The icons TuxBlox ships\nInherits=Adwaita,hicolor\n'
    printf 'Directories=%s\n' "$$(printf '%s\n' "$$iconDirs" | paste -sd,)"
    printf '%s\n' "$$iconDirs" | while IFS= read -r iconDir; do
        printf '\n'
        awk -v section="[$$iconDir]" '
            $$0 == section { inside = 1; print; next }
            inside && /^\[/ { exit }
            inside { print }
        ' /build/gtk4/gtk/icons/hicolor.index.theme
    done
} > "$$themeDir/index.theme"

themeFailed=0
sections="$$(grep -c '^\[.*\]$$' "$$themeDir/index.theme")"
sizes="$$(grep -c '^Size=' "$$themeDir/index.theme")"
if [ "$$sections" -ne "$$((sizes + 1))" ]; then
    printf 'ERROR: the theme index has %s folder descriptions but %s sizes, so one of them says nothing\n' "$$((sections - 1))" "$$sizes" >&2
    themeFailed=1
fi
while IFS= read -r iconDir; do
    relDir="$${iconDir#"$$themeDir"/}"
    if ! grep -q "^\[$$relDir\]$$" "$$themeDir/index.theme"; then
        printf 'ERROR: icon folder %s is not listed in the theme index, so nothing in it can be found\n' "$$relDir" >&2
        themeFailed=1
    fi
done < <(find "$$themeDir" -mindepth 2 -type d)
while IFS= read -r svg; do
    name="$$(basename "$$svg" .svg)"
    if ! find "$$themeDir" -name "$$name.png" -o -name "$$name.symbolic.png" | grep -q .; then
        printf 'ERROR: %s has no rendered twin, so that icon would draw as a missing image\n' "$$svg" >&2
        themeFailed=1
    fi
done < <(find "$$themeDir/scalable" -name '*.svg')
if [ ! -e "$$themeDir/16x16/status/image-missing.png" ]; then
    printf 'ERROR: the icon theme has no image-missing\n' >&2
    themeFailed=1
fi
if [ "$$themeFailed" -ne 0 ]; then
    exit 1
fi
printf ':: %s icons in %s folders\n' "$$(find "$$themeDir" -type f ! -name index.theme | wc -l)" "$$(find "$$themeDir" -mindepth 2 -type d | wc -l)"

printf ':: Verifying both GDK backends are present\n'
gtkSo="$$LibDir/libgtk-4.so"
for backend in gdk_wayland gdk_x11; do
    count="$$(nm -D --defined-only "$$gtkSo" | grep -c "$$backend" || true)"
    if [[ "$$count" -lt 5 ]]; then
        printf 'ERROR: libgtk-4 exports only %s %s symbols -- that backend did not build\n' "$$count" "$$backend" >&2
        exit 1
    fi
    printf ':: %s symbols: %s\n' "$$backend" "$$count"
done
endef
export UI_BUILD

define UI_PACKAGE
set -euo pipefail

source /src/ui/versions.env

Prefix=/opt/tuxblox-ui
LibRel=lib/x86_64-linux-gnu
StageDir=/tmp/ui-stack-stage
OutDir=/out
Tarball="ui-stack-$${UI_STACK_VERSION}-x86_64.tar.zst"
export PATH="$$Prefix/bin:$$PATH"

HostAllowList=(
    '^libc\.so\.6$$' '^libm\.so\.6$$' '^libdl\.so\.2$$' '^libpthread\.so\.0$$' '^librt\.so\.1$$' '^libresolv\.so\.2$$'
    '^ld-linux-x86-64\.so\.2$$'
    '^libX11\.so\.6$$' '^libX11-xcb\.so\.1$$' '^libXau\.so\.6$$' '^libXdmcp\.so\.6$$' '^libXext\.so\.6$$' '^libXrender\.so\.1$$'
    '^libXi\.so\.6$$' '^libXrandr\.so\.2$$' '^libXcursor\.so\.1$$' '^libXdamage\.so\.1$$' '^libXfixes\.so\.3$$' '^libXinerama\.so\.1$$'
    '^libxcb\.so\.1$$' '^libxcb-render\.so\.0$$' '^libxcb-shm\.so\.0$$'
    '^libGL\.so\.1$$' '^libEGL\.so\.1$$' '^libGLdispatch\.so\.0$$'
)

isElf() {
    [ "$$(head -c4 "$$1" | od -An -tx1 | tr -d ' \n')" = "7f454c46" ]
}

isAllowedHostLibrary() {
    local name="$$1" pattern
    for pattern in "$${HostAllowList[@]}"; do
        if [[ "$$name" =~ $$pattern ]]; then
            return 0
        fi
    done
    return 1
}

stageRuntime() {
    printf ':: Staging runtime files\n'
    rm -rf "$$StageDir"
    mkdir -p "$$StageDir/$$LibRel" "$$StageDir/share/glib-2.0" "$$StageDir/fonts"
    cp -a "$$Prefix/$$LibRel/." "$$StageDir/$$LibRel/"
    rm -rf "$$StageDir/$$LibRel/pkgconfig" "$$StageDir/$$LibRel/glib-2.0/include" "$$StageDir/$$LibRel/gdk-pixbuf-2.0"
    rm -f "$$StageDir/$$LibRel"/libgirepository-2.0.so* "$$StageDir/$$LibRel"/libxkbregistry.so*
    cp -a "$$Prefix/fonts/." "$$StageDir/fonts/"
    cat > "$$StageDir/fonts/fonts.conf" <<'FONTS'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "urn:fontconfig:fonts.dtd">
<fontconfig>
    <dir prefix="relative">.</dir>
    <cachedir prefix="xdg">tuxblox/fontconfig</cachedir>
    <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
    <!-- Spelled out because the desktop's own file says "conf.d" and the fontconfig this stack
         carries resolves that against the wrong directory, so it silently loaded none of the rules
         there. Bitmap scaling is one of them, and without it a colour emoji is drawn at the size
         its font stores it at: ten times the line it sits on. -->
    <include ignore_missing="yes">/etc/fonts/conf.d</include>
</fontconfig>
FONTS
    cp -a "$$Prefix/share/glib-2.0/schemas" "$$StageDir/share/glib-2.0/schemas"
    glib-compile-schemas "$$StageDir/share/glib-2.0/schemas"
    cp -a "$$Prefix/share/icons" "$$StageDir/share/icons"
    if [ ! -e "$$StageDir/share/icons/TuxBlox/index.theme" ] || [ ! -e "$$StageDir/share/icons/TuxBlox/16x16/status/image-missing.png" ]; then
        printf 'ERROR: the icon theme is missing from the stack\n' >&2
        exit 1
    fi
    if ! ls "$$StageDir"/fonts/AdwaitaSans-*.ttf >/dev/null 2>&1 || ! ls "$$StageDir"/fonts/AdwaitaMono-*.ttf >/dev/null 2>&1; then
        printf 'ERROR: the stack is missing AdwaitaSans or AdwaitaMono\n' >&2
        exit 1
    fi
    find "$$StageDir" \( -name '*.a' -o -name '*.la' \) -delete
}

rewriteRpaths() {
    local f dir up part parts
    printf ':: Rewriting RPATH on every shipped ELF file\n'
    while IFS= read -r -d '' f; do
        isElf "$$f" || continue
        patchelf --print-rpath "$$f" >/dev/null 2>&1 || continue
        dir="$$(dirname "$${f#"$$StageDir"/}")"
        up=""
        if [ "$$dir" != "." ]; then
            IFS=/ read -ra parts <<<"$$dir"
            for part in "$${parts[@]}"; do up="../$$up"; done
        fi
        strip --strip-unneeded "$$f"
        patchelf --force-rpath --set-rpath "\$$ORIGIN:\$$ORIGIN/$${up}$${LibRel}" "$$f"
    done < <(find "$$StageDir/lib" -type f -print0)
}

checkNeededLibraries() {
    local f needed failed=0
    local hostLines=""
    printf ':: Checking every DT_NEEDED entry is shipped or on the host allow-list\n'
    while IFS= read -r -d '' f; do
        isElf "$$f" || continue
        while IFS= read -r needed; do
            [ -n "$$needed" ] || continue
            if [ -e "$$StageDir/$$LibRel/$$needed" ]; then
                continue
            fi
            if isAllowedHostLibrary "$$needed"; then
                hostLines+="$$needed"$$'\n'
                continue
            fi
            printf 'ERROR: %s needs %s, which is neither shipped nor on the allow-list\n' "$${f#"$$StageDir"/}" "$$needed" >&2
            failed=1
        done < <(readelf -d "$$f" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p')
    done < <(find "$$StageDir/lib" -type f -print0)
    if [ "$$failed" -ne 0 ]; then
        exit 1
    fi
    printf ':: Host libraries the stack relies on:\n'
    printf '%s' "$$hostLines" | sort -u | sed 's/^/::   /'
}

maxSymbolVersion() {
    local prefix="$$1" f
    while IFS= read -r -d '' f; do
        isElf "$$f" || continue
        objdump -T "$$f" 2>/dev/null | grep -o "$${prefix}_[0-9][0-9.]*" || true
    done < <(find "$$StageDir/lib" -type f -print0) | sort -uV | tail -1
}

checkSymbolVersions() {
    local glibcMax glibcxxMax
    printf ':: Checking the symbol versions the stack requires\n'
    glibcMax="$$(maxSymbolVersion GLIBC)"
    glibcxxMax="$$(maxSymbolVersion GLIBCXX)"
    printf '::   highest GLIBC: %s\n' "$${glibcMax:-none}"
    printf '::   highest GLIBCXX: %s\n' "$${glibcxxMax:-none}"
    if [ -n "$$glibcxxMax" ]; then
        printf 'ERROR: something in the stack needs libstdc++ symbols (%s); it must be linked statically instead\n' "$$glibcxxMax" >&2
        exit 1
    fi
    if [ "$$(printf '%s\n%s\n' "$$glibcMax" "GLIBC_2.31" | sort -V | tail -1)" != "GLIBC_2.31" ]; then
        printf 'ERROR: the stack needs %s, above the glibc 2.31 floor\n' "$$glibcMax" >&2
        exit 1
    fi
}

stageVersionLibrary() {
    printf ':: Building version.so\n'
    "$${CXX:-g++}" -std=c++20 -O2 -fPIC -shared -Wl,--no-undefined \
        -static-libstdc++ -static-libgcc \
        -I /src/release/src \
        -DTUXBLOX_COMPONENT_VERSION="\"$${LIBTUXBLOX_VERSION:?shared/ui/versions.env sets the component version}\"" \
        -DTUXBLOX_RELEASE="\"$${TUXBLOX_RELEASE:-}\"" \
        -DTUXBLOX_CHANNEL="\"$${TUXBLOX_CHANNEL:-}\"" \
        -o "$$StageDir/version.so" \
        /src/release/src/stack_version.cpp /src/release/src/release_stamp.cpp
}

writeTarball() {
    printf ':: Writing %s\n' "$$Tarball"
    mkdir -p "$$OutDir"
    printf '%s\n' "$${LIBADWAITA_COMMIT:?the Makefile passes the full submodule commit}" > "$$StageDir/LIBADWAITA_COMMIT"
    printf '%s\n' "$${LIBTUXBLOX_VERSION:?shared/ui/versions.env sets the component version}" > "$$StageDir/VERSION"
    tar -C "$$StageDir" --sort=name --mtime="@$${SOURCE_DATE_EPOCH:?the Makefile exports the commit date}" \
        --owner=0 --group=0 --numeric-owner \
        -cf - lib share fonts version.so LIBADWAITA_COMMIT VERSION | zstd -19 -T0 -q -o "$$OutDir/$$Tarball" -f
    python3 /src/tools/check-archive.py --epoch "$$SOURCE_DATE_EPOCH" "$$OutDir/$$Tarball"
}

writeDevTree() {
    local devDir="$$OutDir/dev" pc
    printf ':: Writing the dev tree\n'
    rm -rf "$$devDir"
    mkdir -p "$$devDir/lib/pkgconfig"
    cp -a "$$Prefix/include" "$$devDir/include"
    cp -a "$$StageDir/$$LibRel" "$$devDir/$$LibRel"
    for arch in "$$Prefix/$$LibRel"/*/include; do
        [ -d "$$arch" ] || continue
        mkdir -p "$$devDir/$$LibRel/$$(basename "$$(dirname "$$arch")")"
        cp -a "$$arch" "$$devDir/$$LibRel/$$(basename "$$(dirname "$$arch")")/"
    done
    for pc in "$$Prefix/$$LibRel"/pkgconfig/*.pc "$$Prefix"/lib/pkgconfig/*.pc "$$Prefix"/share/pkgconfig/*.pc; do
        [ -e "$$pc" ] || continue
        sed "s|$$Prefix|\$${pcfiledir}/../..|g" "$$pc" > "$$devDir/lib/pkgconfig/$$(basename "$$pc")"
    done
}

stageRuntime
rewriteRpaths
stageVersionLibrary
checkNeededLibraries
checkSymbolVersions
writeTarball
writeDevTree
endef
export UI_PACKAGE

$(UI): shared/ui/versions.env shared/ui/sources.list Makefile | $(PODMAN)
	@if [ ! -e $(LIBADWAITA_DIR)/meson.build ]; then \
	    echo "shared/ui/libadwaita is empty. Run: git submodule update --init shared/ui/libadwaita" >&2; \
	    exit 1; \
	fi
	$(ROOT)/shared/ui/fetch-sources.sh
	@mkdir -p $(UI_DIST)/prefix $(UI_DIST)/work $(UI_DIST)/out $(CCACHE_DIR)
	printf '%s' "$$UI_BUILD" | podman run -i --rm --userns=keep-id --network=none \
	    -v $(ROOT)/shared:/src:ro -v $(ROOT)/shared/ui/sources:/sources:ro \
	    -v $(UI_DIST)/prefix:/opt/tuxblox-ui -v $(UI_DIST)/work:/build \
	    -e JOBS="$(JOBS)" -e LIBADWAITA_SOURCE="$(LIBADWAITA_SHORT)" \
	    -e CC=gcc-14 -e CXX=g++-14 \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G \
	    tuxblox-builder bash -s
	printf '%s' "$$UI_PACKAGE" | podman run -i --rm --userns=keep-id --network=none \
	    -v $(ROOT)/shared:/src:ro -v $(UI_DIST)/prefix:/opt/tuxblox-ui:ro -v $(UI_DIST)/out:/out \
	    -e LIBADWAITA_SOURCE="$(LIBADWAITA_SHORT)" -e LIBADWAITA_COMMIT="$(LIBADWAITA_HEAD)" \
	    -e CXX=g++-14 -e TUXBLOX_RELEASE="$(RELEASE)" -e TUXBLOX_CHANNEL="$(CHANNEL)" \
	    -e SOURCE_DATE_EPOCH="$(SOURCE_DATE_EPOCH)" \
	    tuxblox-builder bash -s
	rm -rf $(UI_DIST)/dev && mv $(UI_DIST)/out/dev $(UI_DIST)/dev
	rm -f $(UI_DIST)/ui-stack-*.tar.zst
	mv $(UI_DIST)/out/ui-stack-*.tar.zst $(UI_DIST)/
	rm -rf $(UI_DIST)/out

$(INSTALLER): $(INSTALLER_SRC) $(SHARED_SRC) $(LIBS) $(UI) $(TUXBLOX_ICONS) | $(PODMAN)
	@cd installer && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$$' build/CMakeCache.txt; then rm -rf build; fi
	@mkdir -p $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" \
	    -e UI_STACK_ARG="$(UI_ARGS)" \
	    -e LIBADWAITA_ARG="-DTUXBLOX_LIBADWAITA_COMMIT=$(LIBADWAITA_COMMIT)" \
	    -e APPIMAGE_ARG="-DTUXBLOX_APPIMAGE=$(if $(TUXBLOX_APPIMAGE),ON,OFF)" \
	    -e TUXBLOX_BUILD_VERSION="$(shell cat installer/VERSION 2>/dev/null)" \
	    -e TUXBLOX_RELEASE="$(RELEASE)" -e TUXBLOX_CHANNEL="$(CHANNEL)" \
	    -v $(ROOT)/installer:/src:Z -v $(ART)/shared:/shared:ro,z \
	    -v $(ROOT)/assets:/assets:ro,z -v $(ASSETS_DIR):/assets-build:ro,z \
	    -v $(ROOT)/shared/crypto:/shared-crypto:ro,z -v $(ROOT)/shared/release:/shared-release:ro,z \
	    -v $(ROOT)/shared/flags:/shared-flags:ro,z -v $(ROOT)/shared/i18n:/shared-i18n:ro,z \
	    -v $(ROOT)/shared/translations:/shared-translations:ro,z $(UI_MOUNTS) \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_ASSETS=/assets -DTUXBLOX_ASSETS_BUILD=/assets-build -DTUXBLOX_SHARED_DIR=/shared -DTUXBLOX_SHARED_CRYPTO=/shared-crypto/src \
	        -DTUXBLOX_SHARED_RELEASE=/shared-release/src -DTUXBLOX_SHARED_FLAGS=/shared-flags \
	        -DTUXBLOX_I18N_DIR=/shared-i18n -DTUXBLOX_TRANSLATIONS_DIR=/shared-translations \
	        $${UI_STACK_ARG:-} $${LIBADWAITA_ARG:-} $${APPIMAGE_ARG:-} && cmake --build build -j"$(JOBS)"'
ifndef TUXBLOX_HEADLESS_ONLY
	cd installer && ./append-payload.sh build/TuxBloxInstaller $(UI) build/TuxBloxInstaller-ui build/TuxBloxInstaller
	@if [ -z "$$TUXBLOX_SKIP_SMOKE" ]; then cd installer && ./smoke-test.sh build/TuxBloxInstaller; \
	 else echo "!! TUXBLOX_SKIP_SMOKE set, the fresh-install smoke test was NOT run" >&2; fi
endif
	@mkdir -p $(ART)
	rm -rf $(ART)/installer && mv installer/build $(ART)/installer
	cp $(ART)/installer/TuxBloxInstaller $@

$(STUDIO_MCP): $(STUDIO_MCP_SRC) $(LAUNCHER_SRC) $(SHARED_SRC) $(LIBS) | $(PODMAN)
	@cd studio-mcp && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$$' build/CMakeCache.txt; then rm -rf build; fi
	@mkdir -p $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" \
	    -e TUXBLOX_BUILD_VERSION="$(shell cat studio-mcp/VERSION 2>/dev/null)" \
	    -e TUXBLOX_RELEASE="$(RELEASE)" -e TUXBLOX_CHANNEL="$(CHANNEL)" \
	    -v $(ROOT)/studio-mcp:/src:Z -v $(ROOT)/launcher:/launcher:ro,z \
	    -v $(ROOT)/shared/release:/shared-release:ro,z \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_LAUNCHER_DIR=/launcher -DTUXBLOX_SHARED_RELEASE=/shared-release/src \
	        && cmake --build build -j"$(JOBS)" && cd build && ctest --output-on-failure'
	@mkdir -p $(ART)
	rm -rf $(ART)/studio-mcp && mv studio-mcp/build $(ART)/studio-mcp
	cp $(ART)/studio-mcp/studio-mcp $@

$(LAUNCHER): $(LAUNCHER_SRC) $(SHARED_SRC) $(LIBS) $(UI) $(ASSET_SVGS) $(TUXBLOX_ICONS) | $(PODMAN)
	@cd launcher && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$$' build/CMakeCache.txt; then \
	    echo ":: Dropping stale host-configured launcher/build"; rm -rf build; fi
	@cd launcher && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$$' build/CMakeCache.txt; then \
	    echo ":: Dropping launcher/build configured for another generator"; rm -rf build; fi
	@mkdir -p $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" \
	    -e LIBADWAITA_ARG="-DTUXBLOX_LIBADWAITA_COMMIT=$(LIBADWAITA_COMMIT)" \
	    -e FLATPAK_ARG="-DTUXBLOX_FLATPAK=$(if $(TUXBLOX_FLATPAK),ON,OFF)" \
	    -e TUXBLOX_BUILD_VERSION="$(shell cat launcher/VERSION 2>/dev/null)" \
	    -e TUXBLOX_RELEASE="$(RELEASE)" -e TUXBLOX_CHANNEL="$(CHANNEL)" \
	    -v $(ROOT)/launcher:/src:Z -v $(ROOT)/LICENSE:/LICENSE:ro,z \
	    -v $(ROOT)/assets:/assets:ro,z -v $(ASSETS_DIR):/assets-build:ro,z \
	    -v $(ROOT)/shared/ui/src:/ui-src:ro -v $(UI_DIST)/dev:/ui-dev:ro \
	    -v $(ART)/shared:/shared:ro,z -v $(ROOT)/shared/crypto:/shared-crypto:ro,z \
	    -v $(ROOT)/shared/release:/shared-release:ro,z -v $(ROOT)/shared/flags:/shared-flags:ro,z \
	    -v $(ROOT)/shared/paths:/shared-paths:ro,z -v $(ROOT)/shared/i18n:/shared-i18n:ro,z \
	    -v $(ROOT)/shared/translations:/shared-translations:ro,z \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_UI_SRC=/ui-src -DTUXBLOX_UI_STACK_DEV=/ui-dev -DTUXBLOX_ASSETS=/assets -DTUXBLOX_ASSETS_BUILD=/assets-build -DTUXBLOX_SHARED_DIR=/shared \
	        -DTUXBLOX_SHARED_CRYPTO=/shared-crypto/src -DTUXBLOX_SHARED_RELEASE=/shared-release/src \
	        -DTUXBLOX_SHARED_FLAGS=/shared-flags -DTUXBLOX_SHARED_PATHS=/shared-paths/src \
	        -DTUXBLOX_I18N_DIR=/shared-i18n -DTUXBLOX_TRANSLATIONS_DIR=/shared-translations \
	        $${LIBADWAITA_ARG:-} $${FLATPAK_ARG:-} && cmake --build build -j"$(JOBS)"'
	@if [ ! -f launcher/build/TuxBloxLauncher ]; then echo "!! launcher/build/TuxBloxLauncher was not built" >&2; exit 1; fi
	@if [ -z "$$TUXBLOX_SKIP_SMOKE" ]; then cd launcher && ./smoke-test.sh build/TuxBloxLauncher $(UI); \
	 else echo "!! TUXBLOX_SKIP_SMOKE set, the fresh-install smoke test was NOT run" >&2; fi
	@mkdir -p $(ART)
	rm -rf $(ART)/launcher && mv launcher/build $(ART)/launcher
	cp $(ART)/launcher/TuxBloxLauncher $@
	rm -rf $(NATIVE)/libtuxblox && mkdir -p $(NATIVE)/libtuxblox
	tar --zstd -xf $(UI) -C $(NATIVE)/libtuxblox

define BUILD_ENTRY_POINT
set -euo pipefail
ROOT="$(ROOT)"
ProtonDir="$${1:?the Makefile passes the proton build dir}"
: "$${TUXBLOX_COMPAT_VERSION:?TUXBLOX_COMPAT_VERSION must be set}"


Work="$$(mktemp -d)"
trap 'rm -rf "$$Work"' EXIT
mkdir -p "$$Work/src" "$$Work/out"

cp -a "$$ROOT"/compat/tuxblox/src/. "$$Work/src/"
mkdir -p "$$Work/src/shared_paths" "$$Work/src/shared_flags" "$$Work/src/shared_release"
cp -a "$$ROOT"/shared/paths/src/. "$$Work/src/shared_paths/"
cp "$$ROOT"/shared/flags/FlagManager.cpp "$$ROOT"/shared/flags/FlagManager.h "$$Work/src/shared_flags/"
cp "$$ROOT"/shared/release/src/release_stamp.cpp "$$ROOT"/shared/release/src/release_stamp.h "$$Work/src/shared_release/"
cp "$$ROOT"/shared/crypto/src/ca_bundle.cpp "$$ROOT"/shared/crypto/src/ca_bundle.h "$$Work/src/shared_flags/"

CaHeader="$$ROOT/build/.artifacts/launcher/generated/tuxblox_ca_bundle_pem.h"
if [ ! -f "$$CaHeader" ]; then
    echo "missing $$CaHeader -- the launcher must build first" >&2
    exit 1
fi
cp "$$CaHeader" "$$Work/src/"

"$$ROOT/compat/tuxblox/tools/embed-data.py" "$$ROOT/compat/tuxblox/data" "$$Work/src/embedded_data.h"
"$$ROOT/shared/flags/tools/embed-flags.py" "$$ROOT/shared/flags/DefaultFlags-x86_64.json" \
    "$$Work/src/tuxblox_default_flags.h" kTuxBloxDefaultFlagsJson

podman run --rm --userns=keep-id -v "$$Work:/work:Z" -w /work/src \
    -v "$$ROOT/build/.artifacts/shared:/shared:ro,z" \
    -e TUXBLOX_COMPAT_VERSION -e TUXBLOX_RELEASE -e TUXBLOX_CHANNEL \
    tuxblox-builder sh -c 'g++ -std=c++17 -O2 -Wall -Wextra -static-libstdc++ -static-libgcc \
        -I. -Ithird_party -Ishared_flags -Ishared_release -I/shared/include \
        -DTUXBLOX_VERSION="\"$$TUXBLOX_COMPAT_VERSION\"" -DTUXBLOX_RELEASE="\"$$TUXBLOX_RELEASE\"" \
        -DTUXBLOX_CHANNEL="\"$$TUXBLOX_CHANNEL\"" \
        -o /work/out/main $$(find . -name "*.cpp") \
        /shared/lib/libcurl.a /shared/lib/libnghttp2.a /shared/lib/libssl.a \
        /shared/lib/libcrypto.a /shared/lib/libz.a -ldl -lpthread'

install -m 755 "$$Work/out/main" "$$ProtonDir/dist/main"
endef
export BUILD_ENTRY_POINT

define prepare_layer
	@mkdir -p $(PROTON)
	bash $(ROOT)/shared/tools/fetch-sources.sh --manifest $(ROOT)/compat/crypto \
	    --sources-dir $(ROOT)/compat/sources/crypto --label "layer crypto"
	@set -e; . $(ROOT)/compat/crypto/versions.env; \
	mkdir -p $(ROOT)/compat/sources/crypto/src; \
	for p in gmp-$$GMP_VERSION nettle-$$NETTLE_VERSION libtasn1-$$LIBTASN1_VERSION gnutls-$$GNUTLS_VERSION; do \
	    [ -d "$(ROOT)/compat/sources/crypto/src/$$p" ] && continue; \
	    echo ":: Extracting $$p"; \
	    tar -xf $(ROOT)/compat/sources/crypto/$$p.tar.* -C $(ROOT)/compat/sources/crypto/src; \
	    d=$(ROOT)/compat/sources/crypto/src/$$p; \
	    find $$d -name aclocal.m4 -exec touch {} +; \
	    find $$d -name configure -exec touch {} +; \
	    find $$d -name 'config.h.in' -exec touch {} +; \
	    find $$d -name 'Makefile.in' -exec touch {} +; \
	done
	cd $(PROTON) && $(ROOT)/compat/configure.sh --enable-ccache
	cd $(PROTON) && make -j$(JOBS) TUXBLOX_COMPAT_VERSION=$(COMPAT_VERSION) all-source
	cd $(PROTON)/src-glslang && rm -rf External/spirv-tools External/googletest && \
	    key=$$(sha256sum known_good.json | cut -c1-16); \
	    cache=$(ROOT)/compat/sources/glslang-externals-$$key.tar; \
	    if [ -f "$$cache" ]; then echo ":: Using cached glslang externals"; tar -xf "$$cache"; \
	    else python3 update_glslang_sources.py && mkdir -p "$$(dirname "$$cache")" && \
	        tar -cf "$$cache.part" External && mv -f "$$cache.part" "$$cache" && \
	        echo ":: Cached glslang externals for future builds"; fi
	cd compat/submodules/dxvk-nvapi && git submodule update --init --recursive
	cd $(PROTON) && make wine-x86_64-configure TUXBLOX_COMPAT_VERSION=$(COMPAT_VERSION)
	cd $(PROTON)/obj-wine-x86_64 && make nls/locale.nls
endef

$(COMPAT): $(COMPAT_SRC) $(PATCHES) $(LIBS) $(LAUNCHER) $(WEBVIEW) | $(PODMAN)
	$(prepare_layer)
	cd $(PROTON) && make -j$(JOBS) TUXBLOX_COMPAT_VERSION=$(COMPAT_VERSION)
	@printf '%s' "$$BUILD_ENTRY_POINT" | bash -s -- $(PROTON)
	@if [ -n "$(TUXBLOX_RM_DIST)" ]; then cd $(PROTON) && rm -rf obj-* dst-* && \
	    rm -f .*-configure .*-build .*-post-build .*-dist; fi
	rm -rf $(NATIVE)/compat && mv $(PROTON)/dist $(NATIVE)/compat
	rm -f $(PROTON)/.*-dist
	cp -a LICENSE $(NATIVE)/compat/LICENSE
	rm -rf $(NATIVE)/compat/third_party_licenses
	cp -a third_party_licenses $(NATIVE)/compat/third_party_licenses
	@Skeleton=$(NATIVE)/compat/files/share/default_pfx; \
	    Stamp=$$(printf '%x' $$(( ($(SOURCE_DATE_EPOCH) + 11644473600) * 10000000 ))); \
	    sed -i -e "s/^#time=.*/#time=$$Stamp/" \
	        -e "s/^\(\[.*\]\) [0-9][0-9]*$$/\1 $(SOURCE_DATE_EPOCH)/" "$$Skeleton"/*.reg; \
	    printf '%s\n' "$(SOURCE_DATE_EPOCH)" > "$$Skeleton/.update-timestamp"
	@python3 shared/tools/strip-pe-timestamps.py $(NATIVE)/compat

.PHONY: compat-build
compat-build: $(COMPAT_SRC) $(PATCHES) $(LIBS) | $(PODMAN)
	$(prepare_layer)
	cd $(PROTON) && make -j$(JOBS) TUXBLOX_COMPAT_VERSION=$(COMPAT_VERSION) all-build
	@echo ":: Compiled, not staged. build/compat is untouched, so run make compat to get a usable layer."

.PHONY: webkitgtk
define build_webview
	$(ROOT)/compat/webkitgtk/bundle/fetch-sources.sh
	cd compat/webkitgtk/bundle && ./build.sh
	@mkdir -p $(dir $(WEBVIEW))
	mv compat/webkitgtk/bundle/out/webkitgtk-$(WEBKITGTK_VERSION)-x86_64.tar.xz $(WEBVIEW)
endef

$(WEBVIEW): | $(PODMAN)
	$(build_webview)

webkitgtk: | $(PODMAN)
	$(build_webview)

.PHONY: webkitgtk-host
webkitgtk-host:
	@if [ ! -d $(NATIVE)/compat/files ]; then \
	    echo "No layer in build/compat. Run: make compat" >&2; exit 1; \
	fi
	cd compat/webkitgtk/bundle && ./build-host.sh

define CHECK_HOST_DEPS
set -euo pipefail

BuildDir="$${1:-build}"
StagedLayer="$${2:-staged}"
StackDirName="libtuxblox"
GlibcFloor="GLIBC_2.29"

GlibcNames='^(libc|libm|libdl|libpthread|librt|libresolv|libutil|libanl)\.so\.[0-9]+$$'
HostGraphicsNames='^lib(X11|X11-xcb|Xau|Xdmcp|Xext|Xrender|Xi|Xrandr|Xcursor|Xdamage|Xfixes|Xinerama|Xtst|Xss|xcb|xcb-render|xcb-shm|xcb-dri2|xcb-dri3|xcb-glx|xcb-present|xcb-sync|xcb-xfixes|xcb-randr|xcb-shape|xcb-util|xshmfence|drm|gbm|GL|GLX|EGL|GLdispatch|OpenGL|wayland-client|wayland-cursor|wayland-egl|xkbcommon|xkbcommon-x11)\.so\.[0-9]+$$'

Binaries=(
    "TuxBloxInstaller:no"
    "studio-mcp:no"
    "TuxBloxLauncher:yes"
)
if [[ "$$StagedLayer" == staged ]]; then
    Binaries+=("compat/main:no")
else
    echo ":: the layer was compiled but not staged, so it is not checked here"
fi

Failures=0

note() {
    printf '%s\n' "$$1"
}

problem() {
    printf '%s\n' "$$1" >&2
    Failures=$$((Failures + 1))
}

resolveLoaded() {
    local binary="$$1"
    ldd "$$binary" 2>/dev/null | awk '
        /=>/ {
            path = $$3
            if ($$3 == "not" && $$4 == "found") path = "not-found"
            else if (path == "" || path ~ /^\(0x/) path = "kernel-or-loader"
            print $$1 "\t" path
            next
        }
        /ld-linux|linux-vdso/ { print $$1 "\tkernel-or-loader" }
    '
}

checkBinary() {
    local name="$$1" stackAllowed="$$2"
    local binary="$$BuildDir/$$name"
    local stackRoot="$$BuildDir/$$StackDirName"
    local line libName libPath seen=0 fromStack=0 fromHost=0

    if [[ ! -x "$$binary" ]]; then
        problem "!! missing binary: $$binary"
        return
    fi

    if ! ldd "$$binary" >/dev/null 2>&1; then
        note ":: $$name (not dynamically linked, nothing to resolve)"
        return
    fi

    note ":: $$name"
    while IFS=$$'\t' read -r libName libPath; do
        [[ -z "$$libName" ]] && continue
        seen=$$((seen + 1))

        if [[ "$$libPath" == "kernel-or-loader" || "$$libName" == *"ld-linux"* ]]; then
            continue
        fi
        if [[ "$$libPath" == "not-found" ]]; then
            problem "   !! $$name needs $$libName and it resolves nowhere"
            continue
        fi

        if [[ "$$libPath" == "$$stackRoot/"* || "$$libPath" == *"/$$StackDirName/"* ]]; then
            fromStack=$$((fromStack + 1))
            if [[ "$$stackAllowed" != "yes" ]]; then
                problem "   + $$name must not use the interface stack, but loads $$libName"
            fi
            continue
        fi

        fromHost=$$((fromHost + 1))
        if printf '%s' "$$libName" | grep -qE "$$GlibcNames"; then
            continue
        fi
        if [[ "$$stackAllowed" == "yes" ]] && printf '%s' "$$libName" | grep -qE "$$HostGraphicsNames"; then
            continue
        fi
        problem "   + unexpected dependency on the machine: $$libName ($$libPath)"
    done < <(resolveLoaded "$$binary")

    if [[ $$seen -eq 0 ]]; then
        problem "   !! read nothing from $$name -- ldd gave no output"
        return
    fi
    note "   $$fromStack shipped, $$fromHost from the machine"
}

for entry in "$${Binaries[@]}"; do
    checkBinary "$${entry%%:*}" "$${entry##*:}"
done

while IFS= read -r lib; do
    if objdump -p "$$lib" 2>/dev/null | grep -q 'NEEDED.*libstdc++'; then
        problem "!! $$(basename "$$lib") needs a C++ runtime, so carrying our own in the programs is no longer safe"
    fi
done < <(find "$$BuildDir/$$StackDirName" -name '*.so*' -type f 2>/dev/null)

for entry in "$${Binaries[@]}"; do
    name="$${entry%%:*}"
    binary="$$BuildDir/$$name"
    [[ -x "$$binary" ]] || continue
    if objdump -T "$$binary" 2>/dev/null | grep -q 'GLIBCXX_'; then
        problem "!! $$name still asks the machine for a GLIBCXX version"
    fi
    maxGlibc="$$(objdump -T "$$binary" 2>/dev/null | grep -oE 'GLIBC_[0-9.]+' | sort -V | tail -1 || true)"
    if [[ -z "$$maxGlibc" ]]; then
        note "   $$name asks for no C library version at all"
        continue
    fi
    note "   $$name needs at most $$maxGlibc"
    if [[ "$$(printf '%s\n' "$${maxGlibc#GLIBC_}" "$${GlibcFloor#GLIBC_}" | sort -V | tail -1)" != "$${GlibcFloor#GLIBC_}" ]]; then
        problem "!! $$name needs $$maxGlibc, above the $$GlibcFloor floor TuxBlox promises"
    fi
done

if [[ $$Failures -gt 0 ]]; then
    printf '!! %s problems\n' "$$Failures" >&2
    exit 1
fi
printf ':: every binary loads only what it is expected to\n'
endef
export CHECK_HOST_DEPS

.PHONY: check
check: components
	@printf '%s' "$$CHECK_HOST_DEPS" | bash -s -- $(NATIVE) $(if $(NODIST),nostage,staged)
	@python3 shared/tools/check-archive.py $(UI)

.PHONY: copy-include
copy-include:
	@if [ -d $(ROOT)/include ]; then cp -a $(ROOT)/include/. $(NATIVE)/; fi

.PHONY: deps
deps:
	@echo ":: Installing host build dependencies"
	@if command -v apt-get >/dev/null 2>&1; then sudo apt-get update && sudo apt-get install -y podman curl git uidmap python3 zstd gcc; \
	elif command -v dnf >/dev/null 2>&1; then sudo dnf install -y podman curl git python3 zstd gcc; \
	elif command -v pacman >/dev/null 2>&1; then sudo pacman -S --needed --noconfirm podman curl git python zstd gcc; \
	elif command -v brew >/dev/null 2>&1; then brew install podman curl git python3 zstd gcc; \
	elif command -v apk >/dev/null 2>&1; then sudo apk add podman curl git uidmap python3 zstd gcc; \
	else echo "!! Unknown package manager. Install manually: podman, curl, git, python3, zstd, gcc" >&2; exit 1; fi

.PHONY: clean
clean:
	rm -rf $(BUILD)

.PHONY: distclean
distclean: clean
	rm -rf $(UI_DIST) compat/sources

.PHONY: help
help:
	@echo "make                 build everything"
	@echo "make launcher        the launcher only"
	@echo "make installer       the installer only"
	@echo "make studio-mcp      the Studio MCP gateway only"
	@echo "make appimage        the installer as an AppImage"
	@echo "make flatpak         the whole of TuxBlox as a Flatpak"
	@echo "make assets          render assets/tuxblox.svg into build/assets/ (needs librsvg)"
	@echo "make compat          the compatibility layer only"
	@echo "make webkitgtk       build the webview bundle (hours on a first run)"
	@echo "make webkitgtk-host  just the webview host helper, into an existing layer"
	@echo "make clean           remove build/"
	@echo "make distclean       also remove the interface stack and source caches"
	@echo
	@echo "TUXBLOX_NO_PACKAGES=1  skip the AppImage and the Flatpak in a full build"
	@echo "TUXBLOX_RM_DIST=1    drop the layer's build scratch to free disk"
	@echo "NODIST=1             compile the layer without staging it, for a quick check"
	@echo "JOBS=N               parallel jobs (default: nproc)"
	@echo "TUXBLOX_DONT_INCREMENT_BUILD=1  leave the build counter in VERSION where it is"

