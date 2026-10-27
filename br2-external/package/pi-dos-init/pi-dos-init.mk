PI_DOS_INIT_VERSION = $(shell cat $(BR2_EXTERNAL_PI_DOS_PATH)/../VERSION)
PI_DOS_INIT_SITE = $(BR2_EXTERNAL_PI_DOS_PATH)/../src/init
PI_DOS_INIT_SITE_METHOD = local
# Original project license has not yet been selected.
PI_DOS_INIT_LICENSE = unknown
PI_DOS_INIT_DEPENDENCIES = util-linux parted dosfstools dhcpcd chrony tzdata alsa-lib

define PI_DOS_INIT_BUILD_CMDS
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror \
		-DPI_DOS_VERSION='"$(PI_DOS_INIT_VERSION)"' \
		-I$(BR2_EXTERNAL_PI_DOS_PATH)/../src/common \
		$(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/cpu_profiles.c \
		$(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/video_profiles.c \
		$(BR2_EXTERNAL_PI_DOS_PATH)/../src/common/midi_profiles.c \
		$(@D)/init.c $(@D)/startup.c $(@D)/storage.c $(@D)/diagnostics.c $(@D)/network.c $(@D)/timezone.c $(@D)/config.c $(@D)/video.c $(@D)/mixer.c $(@D)/midi.c $(@D)/power_button.c -o $(@D)/pi-dos-init $(TARGET_LDFLAGS) \
		-lfdisk -lparted-fs-resize -lparted -lasound
	$(TARGET_CC) $(TARGET_CFLAGS) -std=c11 -Wall -Wextra -Werror \
		$(@D)/network-dns.c -o $(@D)/network-dns $(TARGET_LDFLAGS)
endef

define PI_DOS_INIT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 0755 $(@D)/pi-dos-init $(TARGET_DIR)/init
	$(INSTALL) -D -m 0755 $(@D)/network-dns $(TARGET_DIR)/pi-dos/network-dns
endef

$(eval $(generic-package))
