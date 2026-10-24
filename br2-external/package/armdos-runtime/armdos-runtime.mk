ARMDOS_RUNTIME_VERSION = 4f19017c5f565dc40d01fede0f1382892e7243d7
ARMDOS_RUNTIME_SOURCE = dosbox-x-$(ARMDOS_RUNTIME_VERSION).tar.gz
ARMDOS_RUNTIME_SITE = $(call github,joncampbell123,dosbox-x,$(ARMDOS_RUNTIME_VERSION))
ARMDOS_RUNTIME_LICENSE = GPL-2.0-or-later
ARMDOS_RUNTIME_LICENSE_FILES = COPYING
ARMDOS_RUNTIME_DEPENDENCIES = sdl2 alsa-lib zlib libpng
ARMDOS_RUNTIME_AUTORECONF = YES
# The top-level build does not use libtool; bundled vs/ dependencies are unused.
# Patching their ltmain.sh files also breaks retries after a configure failure.
ARMDOS_RUNTIME_LIBTOOL_PATCH = NO
ARMDOS_RUNTIME_CONF_OPTS = --enable-sdl2 --disable-sdlnet --disable-libslirp \
	--disable-x11 --disable-opengl --disable-avcodec --disable-printer \
	--disable-freetype --disable-libfluidsynth --disable-mt32
ARMDOS_RUNTIME_CONF_ENV = SDL2_CONFIG=$(STAGING_DIR)/usr/bin/sdl2-config

define ARMDOS_RUNTIME_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_ARMDOS_PATH)/../patches/dosbox-x '*.patch'
	printf '#define PI_DOS_VERSION "%s"\n' \
		"$(shell cat $(BR2_EXTERNAL_ARMDOS_PATH)/../VERSION)" > $(@D)/include/pi_dos_version.h
endef
ARMDOS_RUNTIME_POST_PATCH_HOOKS += ARMDOS_RUNTIME_APPLY_PATCHES

define ARMDOS_RUNTIME_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/dosbox-x $(TARGET_DIR)/armdos/armdos
endef

$(eval $(autotools-package))
