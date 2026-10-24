include $(sort $(wildcard $(BR2_EXTERNAL_ARMDOS_PATH)/package/*/*.mk))

define ARMDOS_SDL2_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_ARMDOS_PATH)/../patches/sdl2 '*.patch'
endef
SDL2_POST_PATCH_HOOKS += ARMDOS_SDL2_APPLY_PATCHES

define ARMDOS_PARTED_APPLY_PATCHES
	$(APPLY_PATCHES) $(@D) $(BR2_EXTERNAL_ARMDOS_PATH)/../patches/parted '*.patch'
endef
PARTED_POST_PATCH_HOOKS += ARMDOS_PARTED_APPLY_PATCHES
HOST_PARTED_POST_PATCH_HOOKS += ARMDOS_PARTED_APPLY_PATCHES

# Upstream SDL2 gates libudev on a udev daemon provider. Use the standalone
# libudev API instead, without adding a device-management daemon to this image.
ifeq ($(BR2_PACKAGE_LIBUDEV_ZERO),y)
SDL2_CONF_OPTS := $(filter-out --disable-libudev,$(SDL2_CONF_OPTS)) --enable-libudev
SDL2_DEPENDENCIES += libudev
# Upstream package rules were expanded before external.mk was included.
$(SDL2_TARGET_CONFIGURE): | libudev
endif
