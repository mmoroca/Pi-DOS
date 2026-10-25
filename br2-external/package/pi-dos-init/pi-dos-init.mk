PI_DOS_INIT_VERSION = $(shell cat $(BR2_EXTERNAL_PI_DOS_PATH)/../VERSION)
PI_DOS_INIT_SITE = $(BR2_EXTERNAL_PI_DOS_PATH)/../src/init
PI_DOS_INIT_SITE_METHOD = local
# Original project license has not yet been selected.
PI_DOS_INIT_LICENSE = unknown
PI_DOS_INIT_DEPENDENCIES = util-linux parted dosfstools

define PI_DOS_INIT_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror \
		-DPI_DOS_VERSION='"$(PI_DOS_INIT_VERSION)"' \
		$(@D)/init.c $(@D)/startup.c $(@D)/storage.c $(@D)/diagnostics.c -o $(@D)/pi-dos-init $(TARGET_LDFLAGS) \
		-lfdisk -lparted-fs-resize -lparted
endef

define PI_DOS_INIT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/pi-dos-init $(TARGET_DIR)/init
endef

$(eval $(generic-package))
