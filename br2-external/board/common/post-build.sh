#!/bin/sh
set -eu
target=$1
project="$BR2_EXTERNAL_PI_DOS_PATH/.."
# Buildroot retains files from deselected packages in an existing output tree.
# Remove the previous ext4 boot helpers when upgrading that tree to FAT32.
python3 - "$target" "$BUILD_DIR" <<'PY'
from pathlib import Path
import sys
target, build = map(Path, sys.argv[1:])
if 'BR2_PACKAGE_E2FSPROGS=y\n' not in (target.parent / '.config').read_text():
    for manifest in build.glob('e2fsprogs-*/.files-list.txt'):
        for entry in manifest.read_text().splitlines():
            package, separator, relative = entry.partition(',')
            path = Path(relative)
            if package != 'e2fsprogs' or not separator or path.is_absolute() or '..' in path.parts:
                continue
            installed = target / path
            if installed.is_symlink() or installed.is_file():
                installed.unlink()
PY
install -D -m 0644 "$project/configs/pi-dos.conf" "$target/pi-dos/pi-dos.conf"
install -D -m 0644 "$project/configs/dhcpcd.conf" "$target/etc/dhcpcd.conf"
install -D -m 0644 "$project/configs/chrony.conf" "$target/etc/chrony.conf"
# DHCP lease state and DNS must remain writable after the root remount.
mkdir -p "$target/var/db"
# This shell-free skeleton does not provide the usual /var/run link. dhcpcd
# is compiled to keep its PID/control sockets under /var/run/dhcpcd.
ln -sfn /run "$target/var/run"
rm -rf "$target/var/db/dhcpcd"
ln -s /run/dhcpcd-state "$target/var/db/dhcpcd"
rm -f "$target/etc/resolv.conf"
ln -s /run/resolv.conf "$target/etc/resolv.conf"
# Remove the obsolete generated template from incremental build output.
rm -f "$target/pi-dos/DOSBOX.CONF" "$target/pi-dos/DOSBOX.CFG" "$target/pi-dos/TIMEZONE.CFG"
python3 - "$project/configs/pi-dos.conf" "$target/pi-dos/PIDOS.CFG" <<'PY'
from pathlib import Path
import sys
source, destination = map(Path, sys.argv[1:])
text = ('# Pi-DOS persistent configuration. Changes take effect after reboot.\n'
        '# Edit C:\\SYSTEM\\PIDOS.CFG; existing files are preserved on BOOT updates.\n'
        '# Optional [autoexec] commands run after system setup and C:\\AUTOEXEC.BAT.\n\n'
        '[pidos]\n'
        '# First-boot welcome: pending, clock review after reboot, or done.\n'
        'first_boot=pending\n'
        '# Select the board CPU default once, before the first DOS session.\n'
        'cpu_default=pending\n'
        '# Local IANA timezone; daylight saving is automatic.\n'
        '# Examples: Europe/Madrid, Atlantic/Canary, America/New_York, Asia/Tokyo.\n'
        'timezone=Europe/Madrid\n'
        '# Physical audio: hdmi (default), jack (Pi 3/4), usb (external DAC).\n'
        'audio_output=hdmi\n'
        '# Monitor: crt (follow DOS timings, default) or lcd (fixed mode).\n'
        'monitor=crt\nlcd_mode=1024x768\nlcd_aspect=4:3\n'
        '# SDL2 linear filtering for LCD scaling; disabled by default.\n'
        'lcd_linear=false\n'
        '# LCD host CPU/temperature overlay, independent of diagnostic logs.\n'
        'lcd_hud=false\n'
        '# MIDI profile: sc55 (default), none, roland-gm, mt32, cm32l, external.\n'
        'midi_profile=sc55\nmidi_port=\nmidi_delaysysex=false\n'
        '# Mixer percentages, left:right (0=mute, 100=normal, maximum 200).\n'
        'mixer_master=100:100\nmixer_sb=100:100\nmixer_fm=100:100\n'
        'mixer_gus=100:100\nmixer_cms=100:100\nmixer_speaker=100:100\n'
        'mixer_tandy=100:100\nmixer_tandydac=100:100\nmixer_disney=100:100\n'
        'mixer_midi=100:100\n'
        '# Enable BOOT.LOG, RUNTIME.LOG, NETWORK.LOG and VIDEO.LOG; reboot to apply.\n'
        'diagnostics=false\n\n'
        + source.read_text())
destination.write_bytes(text.replace('\n', '\r\n').encode('ascii'))
PY
python3 - "$project/br2-external/board/raspberrypi/dos" "$target/pi-dos" <<'PY'
from pathlib import Path
import sys
source, destination = map(Path, sys.argv[1:])
for name in ('AUTOEXEC.BAT', 'CONFIG.SYS'):
    (destination / name).write_bytes((source / name).read_text().replace('\n', '\r\n').encode('ascii'))
PY
install -D -m 0644 "$project/br2-external/board/raspberrypi/dos/ARJ.EXE" \
    "$target/pi-dos/drivez/BIN/ARJ.EXE"
install -D -m 0644 "$project/br2-external/board/raspberrypi/dos/PIDOSCFG.EXE" \
    "$target/pi-dos/drivez/BIN/PIDOSCFG.EXE"
python3 "$project/tools/dos/catalogs.py" \
    "$BUILD_DIR"/pi-dos-runtime-*/include/dos_inc.h \
    "$target/usr/share/zoneinfo" "$target/pi-dos/drivez/BIN"
python3 "$project/tools/dos/network-assets.py" --install "$target/pi-dos/drivez"
python3 - "$project/docs/networking.txt" "$target/pi-dos" <<'PYNET'
from pathlib import Path
import sys
source, target = map(Path, sys.argv[1:])
manual = source.read_text().replace('\n', '\r\n').encode('ascii')
(target / 'NETWORK.TXT').write_bytes(manual)
(target / 'drivez/NETWORK/NETWORK.TXT').write_bytes(manual)
PYNET
python3 "$project/tools/dos/midi-assets.py" --install "$target/pi-dos/midi-assets"
install -D -m 0644 "$project/dac-crt.bin" "$target/lib/firmware/edid/dac-crt.bin"
mkdir -p "$target/dosdata" "$target/run" "$target/tmp" "$target/proc" "$target/sys"
# No system shell, getty, login or init scripts are needed by the C supervisor.
test -x "$target/init"
test -x "$target/pi-dos/pi-dos"
test -x "$target/pi-dos/network-dns"
test -x "$target/sbin/dhcpcd"
test -x "$target/usr/sbin/chronyd"
test ! -e "$target/bin/sh"
test ! -e "$target/bin/busybox"
test ! -e "$target/sbin/init"
