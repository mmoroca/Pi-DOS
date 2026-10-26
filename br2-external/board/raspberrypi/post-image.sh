#!/bin/bash
set -euo pipefail
board="$BR2_EXTERNAL_PI_DOS_PATH/board/raspberrypi"
boot="$BINARIES_DIR/pi-dos-boot"
data="$BINARIES_DIR/pi-dos-data"
image_name=pi-dos-arm64-sdcard.img
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then image_name=pi-dos-arm64-diagnostic-sdcard.img; fi
mkdir -p "$boot/overlays" "$data/c/SYSTEM" "$BINARIES_DIR/pi-dos-empty"
# Remove the obsolete empty directory left by earlier image builds.
if [[ -d "$data/d" ]]; then rmdir "$data/d"; fi
mkdir -p "$data/c/PROGRAMS/TOOLS" "$data/c/GAMES" "$data/c/DOCS" "$data/c/TEMP"
install -m 0644 "$BINARIES_DIR/Image" "$boot/Image"
# Refresh generated device trees so incremental builds cannot retain a removed
# board variant or an overlay from a different kernel profile.
rm -f "$boot/"*.dtb "$boot/overlays/"*.dtbo
while IFS= read -r tree; do
    install -m 0644 "$BINARIES_DIR/${tree##*/}" "$boot/$tree"
done < "$board/device-trees.txt"
# Pi 2/3/Zero 2 need VideoCore boot firmware; Pi 4 uses the start4/fixup4 pair.
# Pi 5-family boards supply their firmware through EEPROM.
for firmware in bootcode.bin start.elf fixup.dat start4.elf fixup4.dat LICENCE.broadcom; do
    install -m 0644 "$BINARIES_DIR/rpi-firmware/$firmware" "$boot/"
done
install -m 0644 "$board/config.txt" "$boot/config.txt"
install -m 0644 "$board/cmdline.txt" "$boot/cmdline.txt"
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then
    python3 - "$boot/cmdline.txt" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
words = [word for word in path.read_text().split() if not word.startswith(('loglevel=', 'pi-dos.diagnostics='))]
words += ['loglevel=7', 'ignore_loglevel', 'log_buf_len=1M', 'pi-dos.diagnostics=1']
path.write_text(' '.join(words) + '\n')
PY
fi
printf '@ECHO OFF\r\nECHO reboot>C:\\SYSTEM\\CONTROL.REQ\r\nEXIT\r\n' > "$data/c/SYSTEM/REBOOT.BAT"
printf '@ECHO OFF\r\nEXIT\r\n' > "$data/c/SYSTEM/POWEROFF.BAT"
# Keep the personalized editor writable: PEDIT saves settings in its executable.
install -m 0644 "$board/dos/EDIT.COM" "$data/c/PROGRAMS/TOOLS/EDIT.COM"
# Remove the old generated copy from an incremental image staging tree only.
rm -f "$data/c/SYSTEM/EDIT.COM" "$data/c/SYSTEM/DOSBOX.CONF" \
    "$data/c/SYSTEM/DOSBOX.CFG" "$data/c/SYSTEM/TIMEZONE.CFG"
install -m 0644 "$TARGET_DIR/pi-dos/PIDOS.CFG" "$data/c/SYSTEM/PIDOS.CFG"
if [[ ! -e "$data/c/AUTOEXEC.BAT" && ! -L "$data/c/AUTOEXEC.BAT" ]]; then
    install -m 0644 "$TARGET_DIR/pi-dos/AUTOEXEC.BAT" "$data/c/AUTOEXEC.BAT"
fi
mkdir -p "$data/c/PROGRAMS/DOSZIP"
cp -a "$board/dos/doszip/." "$data/c/PROGRAMS/DOSZIP/"
printf '@ECHO OFF\r\nC:\\PROGRAMS\\DOSZIP\\DZ.EXE /CC:\\PROGRAMS\\DOSZIP %%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9\r\n' > "$data/c/SYSTEM/DZ.BAT"
printf '%s\r\n' \
    'Pi-DOS persistent drive C:' '' \
    'Volume labels: C: SYSTEM, Z: PI-DOS.' \
    'D: is left available for CD-ROM images.' '' \
    'On another computer, open the PI-DOS partition and its c folder.' '' \
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
    'Edit C:\SYSTEM\PIDOS.CFG and reboot to change emulator settings,' \
    'including Sound Blaster hardware. SET BLASTER alone does not change it.' \
    '' 'Local timezone:' \
    'In C:\SYSTEM\PIDOS.CFG, set timezone=Europe/Madrid under [pidos]' \
    '(or another IANA timezone), then reboot. Daylight saving is automatic.' \
    'DOS country/date formatting uses country=34 under [config] in that file.' \
    > "$data/c/README.TXT"
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then
    printf '%s\r\n' '' 'Diagnostic image:' \
        'SYSTEM\BOOT.LOG captures kernel messages and input-device snapshots.' \
        'SYSTEM\RUNTIME.LOG captures SDL/DOSBox standard output and errors.' \
        'SYSTEM\NETWORK.LOG captures wired DHCP and network time output.' \
        'Logs are replaced on every diagnostic boot and need writable C:.' \
        'After reconnecting the keyboard, wait 15 seconds before collecting logs.' \
        >> "$data/c/README.TXT"
fi
cat > "$BINARIES_DIR/genimage.cfg" <<EOF
image boot.vfat {
    vfat { label = "BOOT" extraargs = "-F 32" }
    srcpath = "$boot"
    size = 256M
}
image dosdata.vfat {
    vfat { label = "PI-DOS" extraargs = "-F 32 -s 8 -h 526336" }
    srcpath = "$data"
    size = 512M
}
image $image_name {
    hdimage { partition-table-type = "mbr" disk-signature = 0xa4d05001 }
    partition boot { partition-type = 0x0c bootable = "true" image = "boot.vfat" offset = 1M }
    partition dosdata { partition-type = 0x0c image = "dosdata.vfat" }
}
EOF
"$HOST_DIR/bin/genimage" --rootpath "$BINARIES_DIR/pi-dos-empty" \
    --tmppath "$BUILD_DIR/pi-dos-genimage.tmp" --inputpath "$BINARIES_DIR" \
    --outputpath "$BINARIES_DIR" --config "$BINARIES_DIR/genimage.cfg"
sha256sum "$BINARIES_DIR/$image_name" > "$BINARIES_DIR/$image_name.sha256"
