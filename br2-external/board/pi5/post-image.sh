#!/bin/bash
set -euo pipefail
board="$BR2_EXTERNAL_ARMDOS_PATH/board/pi5"
boot="$BINARIES_DIR/armdos-boot"
data="$BINARIES_DIR/armdos-data"
mkdir -p "$boot/overlays" "$data/c/SYSTEM" "$data/d" "$BINARIES_DIR/armdos-empty"
mkdir -p "$data/c/PROGRAMS/TOOLS" "$data/c/GAMES" "$data/c/DOCS" "$data/c/TEMP"
install -m 0644 "$BINARIES_DIR/Image" "$boot/Image"
install -m 0644 "$BINARIES_DIR/bcm2712-rpi-5-b.dtb" "$boot/"
install -m 0644 "$BINARIES_DIR/bcm2712d0-rpi-5-b.dtb" "$boot/"
install -m 0644 "$BINARIES_DIR/vc4-kms-v3d-pi5.dtbo" "$boot/overlays/"
# Older EEPROM firmware applies this overlay to the base DTB on D0 boards.
# Shipping the newer, complete D0 DTB alone does not cover those bootloaders.
install -m 0644 "$BINARIES_DIR/bcm2712d0.dtbo" "$boot/overlays/"
install -m 0644 "$board/config.txt" "$boot/config.txt"
install -m 0644 "$board/cmdline.txt" "$boot/cmdline.txt"
printf '@ECHO OFF\r\nECHO reboot>C:\\SYSTEM\\CONTROL.REQ\r\nEXIT\r\n' > "$data/c/SYSTEM/REBOOT.BAT"
printf '@ECHO OFF\r\nEXIT\r\n' > "$data/c/SYSTEM/POWEROFF.BAT"
# Keep the personalized editor writable: PEDIT saves settings in its executable.
install -m 0644 "$board/dos/EDIT.COM" "$data/c/PROGRAMS/TOOLS/EDIT.COM"
# Remove the old generated copy from an incremental image staging tree only.
rm -f "$data/c/SYSTEM/EDIT.COM"
install -m 0644 "$TARGET_DIR/armdos/DOSBOX.CONF" "$data/c/SYSTEM/DOSBOX.CONF"
if [[ ! -e "$data/c/AUTOEXEC.BAT" && ! -L "$data/c/AUTOEXEC.BAT" ]]; then
    install -m 0644 "$TARGET_DIR/armdos/AUTOEXEC.BAT" "$data/c/AUTOEXEC.BAT"
fi
mkdir -p "$data/c/PROGRAMS/DOSZIP"
cp -a "$board/dos/doszip/." "$data/c/PROGRAMS/DOSZIP/"
printf '@ECHO OFF\r\nC:\\PROGRAMS\\DOSZIP\\DZ.EXE /CC:\\PROGRAMS\\DOSZIP %%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9\r\n' > "$data/c/SYSTEM/DZ.BAT"
printf '%s\r\n' \
    'Pi-DOS persistent drive C:' '' \
    'SYSTEM          Pi-DOS configuration, lifecycle commands and launchers' \
    'PROGRAMS        Installed applications, one folder per application' \
    'PROGRAMS\TOOLS  Self-contained utilities, including EDIT.COM' \
    'GAMES           Games, one folder per game' \
    'DOCS            Your documents and manuals' \
    'TEMP            Temporary files (TEMP and TMP on writable boots)' '' \
    'Type EDIT to open the editor, or DZ for Doszip Commander.' \
    '' 'Startup customization:' \
    'Edit C:\AUTOEXEC.BAT to set environment variables, extend PATH,' \
    'or start programs automatically. Pi-DOS calls it after standard setup.' \
    'Changes take effect on the next boot. Existing files are preserved;' \
    'a minimal template is created only when the file is missing.' \
    'PATH, TEMP, TMP and BLASTER defaults need no entries in this file.' \
    '' 'Emulated hardware settings:' \
    'Edit C:\SYSTEM\DOSBOX.CONF and reboot to change emulator settings,' \
    'including Sound Blaster hardware. SET BLASTER alone does not change it.' \
    > "$data/c/README.TXT"
cat > "$BINARIES_DIR/genimage.cfg" <<EOF
image boot.vfat {
    vfat { label = "BOOT" extraargs = "-F 32" }
    srcpath = "$boot"
    size = 256M
}
image dosdata.vfat {
    vfat { label = "DOSDATA" extraargs = "-F 32 -s 8 -h 526336" }
    srcpath = "$data"
    size = 512M
}
image armdos-pi5-sdcard.img {
    hdimage { partition-table-type = "mbr" disk-signature = 0xa4d05001 }
    partition boot { partition-type = 0x0c bootable = "true" image = "boot.vfat" offset = 1M }
    partition dosdata { partition-type = 0x0c image = "dosdata.vfat" }
}
EOF
"$HOST_DIR/bin/genimage" --rootpath "$BINARIES_DIR/armdos-empty" \
    --tmppath "$BUILD_DIR/armdos-genimage.tmp" --inputpath "$BINARIES_DIR" \
    --outputpath "$BINARIES_DIR" --config "$BINARIES_DIR/genimage.cfg"
sha256sum "$BINARIES_DIR/armdos-pi5-sdcard.img" > "$BINARIES_DIR/armdos-pi5-sdcard.img.sha256"
