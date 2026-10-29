#!/bin/bash
set -euo pipefail
board="$BR2_EXTERNAL_PI_DOS_PATH/board/raspberrypi"
boot="$BINARIES_DIR/pi-dos-boot"
data="$BINARIES_DIR/pi-dos-data"
image_name=pi-dos-arm64-sdcard.img
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then image_name=pi-dos-arm64-diagnostic-sdcard.img; fi
# This is generated image staging, never the mounted user data volume.
rm -rf "$data"
mkdir -p "$boot/overlays" "$data/SYSTEM" "$BINARIES_DIR/pi-dos-empty"
mkdir -p "$data/PROGRAMS/TOOLS" "$data/GAMES" "$data/DOCS" "$data/TEMP"
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
words = [word for word in path.read_text().split() if not word.startswith('loglevel=')]
words += ['loglevel=7', 'ignore_loglevel', 'log_buf_len=1M']
path.write_text(' '.join(words) + '\n')
PY
fi
printf '@ECHO OFF\r\nECHO reboot>C:\\SYSTEM\\CONTROL.REQ\r\nEXIT\r\n' > "$data/SYSTEM/REBOOT.BAT"
printf '@ECHO OFF\r\nEXIT\r\n' > "$data/SYSTEM/POWEROFF.BAT"
# Keep the personalized editor writable: PEDIT saves settings in its executable.
install -m 0644 "$board/dos/EDIT.COM" "$data/PROGRAMS/TOOLS/EDIT.COM"
python3 - "$BR2_EXTERNAL_PI_DOS_PATH/.." "$data/DOCS" <<'PY'
from pathlib import Path
import sys
project, docs = map(Path, sys.argv[1:])
manual = (project / 'src/doscfg/README.DOS').read_text()
docs.joinpath('PIDOSCFG.TXT').write_bytes(manual.replace('\n', '\r\n').encode('ascii'))
notice = ('Open Watcom runtime used by PIDOSCFG.EXE\n\n'
          'Unmodified compiler/runtime release: 2026-10-01-Build\n'
          'Source: https://github.com/open-watcom/open-watcom-v2/tree/2026-10-01-Build\n'
          'Runtime source is under bld/clib in that repository.\n'
          'Source and release: https://github.com/open-watcom/open-watcom-v2/releases/tag/2026-10-01-Build\n\n'
          + (project / 'LICENSES/OpenWatcom.txt').read_text())
docs.joinpath('WATCOM.TXT').write_bytes(notice.replace('\n', '\r\n').encode('ascii'))
PY
install -m 0644 "$TARGET_DIR/pi-dos/PIDOS.CFG" "$data/SYSTEM/PIDOS.CFG"
mkdir -p "$data/SYSTEM/MIDI"
cp -a "$TARGET_DIR/pi-dos/midi-assets/." "$data/SYSTEM/MIDI/"
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then
    python3 - "$data/SYSTEM/PIDOS.CFG" <<'PY'
from pathlib import Path
import sys
path = Path(sys.argv[1])
config = path.read_bytes()
assert config.count(b'diagnostics=false\r\n') == 1, 'Missing/ambiguous diagnostics default'
path.write_bytes(config.replace(b'diagnostics=false\r\n', b'diagnostics=true\r\n'))
PY
fi
install -m 0644 "$TARGET_DIR/pi-dos/NETWORK.TXT" "$data/DOCS/NETWORK.TXT"
install -m 0644 "$TARGET_DIR/pi-dos/AUTOEXEC.BAT" "$data/AUTOEXEC.BAT"
mkdir -p "$data/PROGRAMS/DOSZIP"
cp -a "$board/dos/doszip/." "$data/PROGRAMS/DOSZIP/"
printf '@ECHO OFF\r\nC:\\PROGRAMS\\DOSZIP\\DZ.EXE /CC:\\PROGRAMS\\DOSZIP %%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9\r\n' > "$data/SYSTEM/DZ.BAT"
printf '%s\r\n' \
    'Pi-DOS persistent drive C:' '' \
    'Volume labels: C: SYSTEM, Z: PI-DOS.' \
    'A: USB floppy; D: USB optical; E: onward: external USB volumes.' '' \
    'On another computer, open the PI-DOS partition and its root directory.' '' \
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
    'Pi-DOS manages C: and Z:. Directory mounts and SUBST mappings are disabled.' \
    'MOUNT lists drives; MOUNT -U E optionally ejects a USB volume.' \
    'Use IMGMOUNT with an available letter for disk or CD images.' \
    'ZIPMOUNT E C:\GAMES\DOOM.ZIP mounts a ZIP with a sibling DOOM.PFS folder.' \
    'The ZIP stays unchanged; saves and changes persist in the .PFS folder.' \
    'Add -ro to disable writes; ZIPMOUNT E -u unmounts after closing files.' \
    'The shell shows uppercase 8.3 aliases; DIR /-Z shows long names.' \
    'Original filenames on disk are preserved; paths ignore case.' \
    'Changes take effect on the next boot. Existing files are preserved;' \
    'a minimal template is created only when the file is missing.' \
    'PATH, TEMP, TMP and BLASTER defaults need no entries in this file.' \
    '' 'Emulated hardware settings:' \
    'Edit C:\SYSTEM\PIDOS.CFG and reboot to change emulator settings,' \
    'including Sound Blaster hardware. SET BLASTER alone does not change it.' \
    'Run PIDOSCFG for CPU, RAM, video, sound, MIDI, mixer and system setup.' \
    'Save and REBOOT to apply hardware changes. Matching mixer channels apply live.' \
    'Setup preserves other settings and saves the previous file as PIDOS.BAK.' \
    'Run VIDEOMOD /P to list host display and DOS BIOS/VESA video modes.' \
    'Use VIDEOMOD /HOST or /DOS to filter the list; /? displays help.' \
    'MIDI ROMs and GM banks are in C:\SYSTEM\MIDI. See DOCS\PIDOSCFG.TXT.' \
    '' 'Local timezone:' \
    'In C:\SYSTEM\PIDOS.CFG, set timezone=Europe/Madrid under [pidos]' \
    '(or another IANA timezone), then reboot. Daylight saving is automatic.' \
    'DOS country/date formatting uses country=34 under [config] in that file.' \
    '' 'File logging:' \
    'In C:\SYSTEM\PIDOS.CFG, set diagnostics=true under [pidos] and reboot.' \
    'BOOT.LOG, RUNTIME.LOG, NETWORK.LOG and VIDEO.LOG are saved in C:\SYSTEM.' \
    'Set diagnostics=false and reboot to stop capture; existing logs remain.' \
    > "$data/README.TXT"
if [[ "${PI_DOS_DIAGNOSTIC:-0}" == 1 ]]; then
    printf '%s\r\n' '' 'Diagnostic image:' \
        'SYSTEM\BOOT.LOG captures kernel messages and input-device snapshots.' \
        'SYSTEM\RUNTIME.LOG captures SDL/DOSBox standard output and errors.' \
        'SYSTEM\NETWORK.LOG captures wired DHCP and network time output.' \
        'SYSTEM\VIDEO.LOG captures video mode selection and presentation details.' \
        'This image enables diagnostics=true under [pidos] in SYSTEM\PIDOS.CFG.' \
        'Logs are replaced on every diagnostic boot and need writable C:.' \
        'After reconnecting the keyboard, wait 15 seconds before collecting logs.' \
        >> "$data/README.TXT"
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
