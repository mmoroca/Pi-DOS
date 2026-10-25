ARMDOS_INIT_VERSION = $(shell cat $(BR2_EXTERNAL_ARMDOS_PATH)/../VERSION)
ARMDOS_INIT_SITE = $(BR2_EXTERNAL_ARMDOS_PATH)/../src/init
ARMDOS_INIT_SITE_METHOD = local
# Original project license has not yet been selected.
ARMDOS_INIT_LICENSE = unknown
ARMDOS_INIT_DEPENDENCIES = util-linux parted dosfstools

define ARMDOS_INIT_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror \
		-DPI_DOS_VERSION='"$(ARMDOS_INIT_VERSION)"' \
		$(@D)/init.c $(@D)/startup.c $(@D)/storage.c $(@D)/diagnostics.c -o $(@D)/armdos-init $(TARGET_LDFLAGS) \
		-lfdisk -lparted-fs-resize -lparted
endef

define ARMDOS_INIT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/armdos-init $(TARGET_DIR)/init
endef

$(eval $(generic-package))
