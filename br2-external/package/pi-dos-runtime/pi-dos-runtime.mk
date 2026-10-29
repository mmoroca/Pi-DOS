PI_DOS_RUNTIME_VERSION = 4f19017c5f565dc40d01fede0f1382892e7243d7
PI_DOS_RUNTIME_SOURCE = dosbox-x-$(PI_DOS_RUNTIME_VERSION).tar.gz
PI_DOS_RUNTIME_SITE = $(call github,joncampbell123,dosbox-x,$(PI_DOS_RUNTIME_VERSION))
PI_DOS_RUNTIME_LICENSE = GPL-2.0-or-later
PI_DOS_RUNTIME_LICENSE_FILES = COPYING
PI_DOS_RUNTIME_DEPENDENCIES = sdl2 alsa-lib zlib libpng fluidsynth util-linux dosfstools exfatprogs
PI_DOS_RUNTIME_AUTORECONF = YES
# The top-level build does not use libtool; bundled vs/ dependencies are unused.
# Patching their ltmain.sh files also breaks retries after a configure failure.
PI_DOS_RUNTIME_LIBTOOL_PATCH = NO
PI_DOS_RUNTIME_CONF_OPTS = --enable-sdl2 --disable-sdlnet --disable-libslirp \
	--disable-x11 --disable-opengl --disable-avcodec --disable-printer \
	--disable-freetype --enable-libfluidsynth --enable-mt32
PI_DOS_RUNTIME_CONF_ENV = SDL2_CONFIG=$(STAGING_DIR)/usr/bin/sdl2-config LIBS="-lblkid"

define PI_DOS_RUNTIME_APPLY_PATCHES
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/drive_policy.h $(@D)/include/pidos_drive_policy.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/system_drive_dos.h $(@D)/src/dos/pidos_system_drive_dos.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_storage_api.h $(@D)/include/pidos_usb_storage_api.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_storage_io.h $(@D)/src/dos/pidos_usb_storage_io.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_storage_linux.h $(@D)/src/dos/pidos_usb_storage_linux.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_storage_dos.h $(@D)/src/dos/pidos_usb_storage_dos.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_floppy_api.h $(@D)/include/pidos_usb_floppy_api.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_floppy_linux.h $(@D)/src/dos/pidos_usb_floppy_linux.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_floppy_dos.h $(@D)/src/dos/pidos_usb_floppy_dos.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_cdrom_linux.h $(@D)/src/dos/pidos_usb_cdrom_linux.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_cdrom_player.h $(@D)/src/dos/pidos_usb_cdrom_player.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_cdrom_backend.h $(@D)/src/dos/pidos_usb_cdrom_backend.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/runtime/usb_cdrom_dos.h $(@D)/src/dos/pidos_usb_cdrom_dos.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/midi_ports.h $(@D)/src/gui/pidos_midi_ports.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/reboot_shortcut.h $(@D)/src/gui/pidos_reboot_shortcut.h
	$(INSTALL) -m 0644 $(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/lcd_video.h $(@D)/include/pidos_lcd_video.h
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_PI_DOS_PATH)/../patches/dosbox-x '*.patch'
	printf '#define PI_DOS_VERSION "%s"\n' \
		"$(shell cat $(BR2_EXTERNAL_PI_DOS_PATH)/../VERSION)" > $(@D)/include/pi_dos_version.h
endef
PI_DOS_RUNTIME_POST_PATCH_HOOKS += PI_DOS_RUNTIME_APPLY_PATCHES

define PI_DOS_RUNTIME_CHECK_MIDI
	grep -q '^#define C_FLUIDSYNTH 1' $(@D)/config.h
	grep -q '^#define C_MT32 1' $(@D)/config.h
endef
PI_DOS_RUNTIME_POST_CONFIGURE_HOOKS += PI_DOS_RUNTIME_CHECK_MIDI

define PI_DOS_RUNTIME_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/src/dosbox-x $(TARGET_DIR)/pi-dos/pi-dos
endef

$(eval $(autotools-package))
