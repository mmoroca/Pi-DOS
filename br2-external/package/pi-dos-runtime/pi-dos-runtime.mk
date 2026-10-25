PI_DOS_RUNTIME_VERSION = 4f19017c5f565dc40d01fede0f1382892e7243d7
PI_DOS_RUNTIME_SOURCE = dosbox-x-$(PI_DOS_RUNTIME_VERSION).tar.gz
PI_DOS_RUNTIME_SITE = $(call github,joncampbell123,dosbox-x,$(PI_DOS_RUNTIME_VERSION))
PI_DOS_RUNTIME_LICENSE = GPL-2.0-or-later
PI_DOS_RUNTIME_LICENSE_FILES = COPYING
PI_DOS_RUNTIME_DEPENDENCIES = sdl2 alsa-lib zlib libpng
PI_DOS_RUNTIME_AUTORECONF = YES
# The top-level build does not use libtool; bundled vs/ dependencies are unused.
# Patching their ltmain.sh files also breaks retries after a configure failure.
PI_DOS_RUNTIME_LIBTOOL_PATCH = NO
PI_DOS_RUNTIME_CONF_OPTS = --enable-sdl2 --disable-sdlnet --disable-libslirp \
	--disable-x11 --disable-opengl --disable-avcodec --disable-printer \
	--disable-freetype --disable-libfluidsynth --disable-mt32
PI_DOS_RUNTIME_CONF_ENV = SDL2_CONFIG=$(STAGING_DIR)/usr/bin/sdl2-config

define PI_DOS_RUNTIME_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_PI_DOS_PATH)/../patches/dosbox-x '*.patch'
	printf '#define PI_DOS_VERSION "%s"\n' \
		"$(shell cat $(BR2_EXTERNAL_PI_DOS_PATH)/../VERSION)" > $(@D)/include/pi_dos_version.h
endef
PI_DOS_RUNTIME_POST_PATCH_HOOKS += PI_DOS_RUNTIME_APPLY_PATCHES

define PI_DOS_RUNTIME_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/dosbox-x $(TARGET_DIR)/pi-dos/pi-dos
endef

$(eval $(autotools-package))
