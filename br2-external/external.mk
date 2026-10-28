include $(sort $(wildcard $(BR2_EXTERNAL_PI_DOS_PATH)/package/*/*.mk))

define PI_DOS_LINUX_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_PI_DOS_PATH)/../patches/linux '*.patch'
endef
LINUX_POST_PATCH_HOOKS += PI_DOS_LINUX_APPLY_PATCHES

define PI_DOS_SDL2_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_PI_DOS_PATH)/../patches/sdl2 '*.patch'
endef
SDL2_POST_PATCH_HOOKS += PI_DOS_SDL2_APPLY_PATCHES

define PI_DOS_PARTED_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_PI_DOS_PATH)/../patches/parted '*.patch'
endef
PARTED_POST_PATCH_HOOKS += PI_DOS_PARTED_APPLY_PATCHES
HOST_PARTED_POST_PATCH_HOOKS += PI_DOS_PARTED_APPLY_PATCHES

define PI_DOS_INSTALL_RPI_FIRMWARE_LICENSE
	$(INSTALL) -D -m 0644 $(@D)/boot/LICENCE.broadcom \
		$(BINARIES_DIR)/rpi-firmware/LICENCE.broadcom
endef
RPI_FIRMWARE_POST_INSTALL_IMAGES_HOOKS += PI_DOS_INSTALL_RPI_FIRMWARE_LICENSE

# Upstream SDL2 gates libudev on a udev daemon provider. Use the standalone
# libudev API instead, without adding a device-management daemon to this image.
ifeq ($(BR2_PACKAGE_LIBUDEV_ZERO),y)
SDL2_CONF_OPTS := $(filter-out --disable-libudev,$(SDL2_CONF_OPTS)) --enable-libudev
SDL2_DEPENDENCIES += libudev
# Upstream package rules were expanded before external.mk was included.
$(SDL2_TARGET_CONFIGURE): | libudev
endif
