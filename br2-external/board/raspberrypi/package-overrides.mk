# Read before Buildroot's package recipes so version-dependent directories and
# download names use the fixed snapshot too. The December 2025 firmware bundled
# by Buildroot 2026.08 causes VL805 initialization timeouts on affected Pi 4s.
override RPI_FIRMWARE_VERSION = c3c104bcfd90a5e6f7c89f00204ac11f1e9ffa6f
override RPI_FIRMWARE_HASH_FILES = $(call qstrip,$(BR2_EXTERNAL_ARMDOS_PATH))/board/raspberrypi/rpi-firmware.hash
