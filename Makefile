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

# Local build settings, gitignored. Not .env: that file is deploy credentials.
-include config.mk

# Build scratch is what makes a rebuild fast. Set TUXBLOX_RM_DIST=1 to free the disk instead.
TUXBLOX_RM_DIST ?=

BUILD := $(ROOT)/build
ART := $(BUILD)/.artifacts
PROTON := $(ART)/proton
UI_DIST := $(ROOT)/shared/ui/dist

LAUNCHER := $(BUILD)/TuxBloxLauncher
INSTALLER := $(BUILD)/TuxBloxInstaller
STUDIO_MCP := $(BUILD)/studio-mcp
COMPAT := $(BUILD)/compat/main
UI_STACK_VERSION := $(shell sed -n 's/^UI_STACK_VERSION=//p' shared/tools/ui-versions.env)
UI := $(UI_DIST)/ui-stack-$(UI_STACK_VERSION)-x86_64.tar.zst
LIBS := $(ART)/shared/lib/libcurl.a
PATCHES := $(ART)/.patches
PODMAN := $(ART)/.podman

COMPAT_VERSION := $(shell cat compat/VERSION 2>/dev/null)
LIBADWAITA_COMMIT := $(shell . $(CURDIR)/shared/tools/ui-libadwaita-commit.sh 2>/dev/null && libadwaitaCommit $(CURDIR)/shared/ui 2>/dev/null)
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
# One job knob: the component builds read their own variable, so JOBS has to reach them too.
export TUXBLOX_MAKE_JOBS := $(JOBS)
export TUXBLOX_COMPAT_VERSION := $(COMPAT_VERSION)

# Each sub-build already uses every core, so running two at once only thrashes.
.NOTPARALLEL:

.PHONY: all
all: components check copy-include

.PHONY: components
components: launcher installer studio-mcp compat

# The interface stack, the shared libraries, patches and the builder image are not targets anyone
# invokes. They are prerequisites, so they build exactly when something needs them.
.PHONY: launcher installer studio-mcp compat deps
launcher: $(LAUNCHER)
installer: $(INSTALLER)
studio-mcp: $(STUDIO_MCP)
compat: $(COMPAT)

$(PODMAN):
	@mkdir -p $(dir $@)
	podman info >/dev/null
	podman build -t tuxblox-old-glibc-builder -f $(ROOT)/Containerfile $(ROOT)
	@touch $@

$(PATCHES): compat/Makefile.in $(wildcard compat/patches/*/*)
	@mkdir -p $(dir $@)
	$(ROOT)/compat/apply-patches.sh
	@touch $@

$(LIBS): $(wildcard shared/tools/libs-*.sh) | $(PODMAN)
	$(ROOT)/shared/tools/libs-build.sh

$(UI): $(wildcard shared/tools/ui-*.sh) shared/tools/ui-versions.env shared/ui/sources.list | $(PODMAN)
	@if [ ! -e $(ROOT)/shared/ui/libadwaita/meson.build ]; then \
	    echo "shared/ui/libadwaita is empty. Run: git submodule update --init shared/ui/libadwaita" >&2; \
	    exit 1; \
	fi
	$(ROOT)/shared/ui/fetch-sources.sh
	$(ROOT)/shared/tools/ui-build.sh

$(INSTALLER): $(INSTALLER_SRC) $(SHARED_SRC) $(LIBS) $(UI) | $(PODMAN)
	@cd installer && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$$' build/CMakeCache.txt; then rm -rf build; fi
	@mkdir -p $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" \
	    -e UI_STACK_ARG="$(UI_ARGS)" \
	    -e LIBADWAITA_ARG="-DTUXBLOX_LIBADWAITA_COMMIT=$(LIBADWAITA_COMMIT)" \
	    -e TUXBLOX_BUILD_VERSION="$(shell cat installer/VERSION 2>/dev/null)" \
	    -v $(ROOT)/installer:/src:Z -v $(ART)/shared:/shared:ro,z \
	    -v $(ROOT)/shared/crypto:/shared-crypto:ro,z -v $(ROOT)/shared/release:/shared-release:ro,z \
	    -v $(ROOT)/shared/flags:/shared-flags:ro,z -v $(ROOT)/shared/i18n:/shared-i18n:ro,z \
	    -v $(ROOT)/shared/translations:/shared-translations:ro,z $(UI_MOUNTS) \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-old-glibc-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_SHARED_DIR=/shared -DTUXBLOX_SHARED_CRYPTO=/shared-crypto/src \
	        -DTUXBLOX_SHARED_RELEASE=/shared-release/src -DTUXBLOX_SHARED_FLAGS=/shared-flags \
	        -DTUXBLOX_I18N_DIR=/shared-i18n -DTUXBLOX_TRANSLATIONS_DIR=/shared-translations \
	        $${UI_STACK_ARG:-} $${LIBADWAITA_ARG:-} && cmake --build build -j"$(JOBS)"'
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
	    -v $(ROOT)/studio-mcp:/src:Z -v $(ROOT)/launcher:/launcher:ro,z \
	    -v $(ROOT)/shared/release:/shared-release:ro,z \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-old-glibc-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_LAUNCHER_DIR=/launcher -DTUXBLOX_SHARED_RELEASE=/shared-release/src \
	        && cmake --build build -j"$(JOBS)" && cd build && ctest --output-on-failure'
	@mkdir -p $(ART)
	rm -rf $(ART)/studio-mcp && mv studio-mcp/build $(ART)/studio-mcp
	cp $(ART)/studio-mcp/studio-mcp $@

$(LAUNCHER): $(LAUNCHER_SRC) $(SHARED_SRC) $(LIBS) $(UI) | $(PODMAN)
	@# A cache configured on the host, or for another generator, cannot be reused in the container.
	@cd launcher && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_CACHEFILE_DIR:INTERNAL=/src/build$$' build/CMakeCache.txt; then \
	    echo ":: Dropping stale host-configured launcher/build"; rm -rf build; fi
	@cd launcher && if [ -f build/CMakeCache.txt ] && ! grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$$' build/CMakeCache.txt; then \
	    echo ":: Dropping launcher/build configured for another generator"; rm -rf build; fi
	@mkdir -p $(CCACHE_DIR)
	podman run --rm --userns=keep-id -e JOBS="$(JOBS)" \
	    -e LIBADWAITA_ARG="-DTUXBLOX_LIBADWAITA_COMMIT=$(LIBADWAITA_COMMIT)" \
	    -e TUXBLOX_BUILD_VERSION="$(shell cat launcher/VERSION 2>/dev/null)" \
	    -v $(ROOT)/launcher:/src:Z -v $(ROOT)/LICENSE:/LICENSE:ro,z \
	    -v $(ROOT)/shared/ui/src:/ui-src:ro -v $(UI_DIST)/dev:/ui-dev:ro \
	    -v $(ART)/shared:/shared:ro,z -v $(ROOT)/shared/crypto:/shared-crypto:ro,z \
	    -v $(ROOT)/shared/release:/shared-release:ro,z -v $(ROOT)/shared/flags:/shared-flags:ro,z \
	    -v $(ROOT)/shared/paths:/shared-paths:ro,z -v $(ROOT)/shared/i18n:/shared-i18n:ro,z \
	    -v $(ROOT)/shared/translations:/shared-translations:ro,z \
	    -v $(CCACHE_DIR):/ccache:z -e CCACHE_DIR=/ccache -e CCACHE_MAXSIZE=30G -w /src \
	    tuxblox-old-glibc-builder bash -c 'cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release \
	        -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
	        -DTUXBLOX_UI_SRC=/ui-src -DTUXBLOX_UI_STACK_DEV=/ui-dev -DTUXBLOX_SHARED_DIR=/shared \
	        -DTUXBLOX_SHARED_CRYPTO=/shared-crypto/src -DTUXBLOX_SHARED_RELEASE=/shared-release/src \
	        -DTUXBLOX_SHARED_FLAGS=/shared-flags -DTUXBLOX_SHARED_PATHS=/shared-paths/src \
	        -DTUXBLOX_I18N_DIR=/shared-i18n -DTUXBLOX_TRANSLATIONS_DIR=/shared-translations \
	        $${LIBADWAITA_ARG:-} && cmake --build build -j"$(JOBS)"'
	@if [ ! -f launcher/build/TuxBloxLauncher ]; then echo "!! launcher/build/TuxBloxLauncher was not built" >&2; exit 1; fi
	@if [ -z "$$TUXBLOX_SKIP_SMOKE" ]; then cd launcher && ./smoke-test.sh build/TuxBloxLauncher $(UI); \
	 else echo "!! TUXBLOX_SKIP_SMOKE set, the fresh-install smoke test was NOT run" >&2; fi
	@mkdir -p $(ART)
	rm -rf $(ART)/launcher && mv launcher/build $(ART)/launcher
	cp $(ART)/launcher/TuxBloxLauncher $@
	rm -rf $(BUILD)/libtuxblox && mkdir -p $(BUILD)/libtuxblox
	tar --zstd -xf $(UI) -C $(BUILD)/libtuxblox

# The layer's own launcher embeds the CA header the launcher build generates.
$(COMPAT): $(COMPAT_SRC) $(PATCHES) $(LIBS) $(LAUNCHER) $(WEBVIEW)
	@mkdir -p $(PROTON)
	cd $(PROTON) && $(ROOT)/compat/configure.sh --enable-ccache
	cd $(PROTON) && make -j$(JOBS) all-source
	$(ROOT)/shared/tools/glslang-externals.sh $(PROTON)/src-glslang
	cd compat/submodules/dxvk-nvapi && git submodule update --init --recursive
	cd $(PROTON) && make wine-x86_64-configure
	cd $(PROTON)/obj-wine-x86_64 && make nls/locale.nls
	cd $(PROTON) && make -j$(JOBS)
	$(ROOT)/compat/tuxblox/build-main.sh $(PROTON)
	@if [ -n "$(TUXBLOX_RM_DIST)" ]; then cd $(PROTON) && rm -rf obj-* dst-*; fi
	rm -rf $(BUILD)/compat && mv $(PROTON)/dist $(BUILD)/compat
	cp -a LICENSE $(BUILD)/compat/LICENSE
	rm -rf $(BUILD)/compat/third_party_licenses
	cp -a third_party_licenses $(BUILD)/compat/third_party_licenses
	$(ROOT)/shared/tools/alias-graphics-driver.sh $(BUILD)/compat/files

# Vendored as a committed tarball, so this is a deliberate rebuild and not part of a normal build.
.PHONY: webkitgtk
define build_webview
	$(ROOT)/compat/webkitgtk/bundle/fetch-sources.sh
	cd compat/webkitgtk/bundle && ./build.sh
	@mkdir -p $(dir $(WEBVIEW))
	mv compat/webkitgtk/bundle/out/webkitgtk-$(WEBKITGTK_VERSION)-x86_64.tar.xz $(WEBVIEW)
endef

# No prerequisites on purpose: an hour and a half of WebKit is never triggered by a bundle script
# changing. It is built when the tarball is missing, and otherwise only by asking for it by name.
$(WEBVIEW): | $(PODMAN)
	$(build_webview)

webkitgtk: | $(PODMAN)
	$(build_webview)

# Rebuilds only the host helper, straight into a layer that is already built.
.PHONY: webkitgtk-host
webkitgtk-host:
	@if [ ! -d $(BUILD)/compat/files ]; then \
	    echo "No layer in build/compat. Run: make compat" >&2; exit 1; \
	fi
	cd compat/webkitgtk/bundle && ./build-host.sh

.PHONY: check
check: components
	$(ROOT)/shared/tools/check-host-deps.sh $(BUILD)

.PHONY: copy-include
copy-include:
	@if [ -d $(ROOT)/include ]; then cp -a $(ROOT)/include/. $(BUILD)/; fi

.PHONY: deps
deps:
	$(ROOT)/shared/tools/install-host-deps.sh

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
	@echo "make compat          the compatibility layer only"
	@echo "make webkitgtk       build the webview bundle (hours on a first run)"
	@echo "make webkitgtk-host  just the webview host helper, into an existing layer"
	@echo "make clean           remove build/"
	@echo "make distclean       also remove the interface stack and source caches"
	@echo
	@echo "TUXBLOX_RM_DIST=1    drop the layer's build scratch to free disk"
	@echo "JOBS=N               parallel jobs (default: nproc)"

