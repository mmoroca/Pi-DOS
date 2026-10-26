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
        '# Local IANA timezone; daylight saving is automatic.\n'
        '# Examples: Europe/Madrid, Atlantic/Canary, America/New_York, Asia/Tokyo.\n'
        'timezone=Europe/Madrid\n'
        '# Enable BOOT.LOG, RUNTIME.LOG, NETWORK.LOG and VIDEO.LOG; reboot to apply.\n'
        'diagnostics=false\n\n'
        + source.read_text())
destination.write_bytes(text.replace('\n', '\r\n').encode('ascii'))
PY
python3 - "$project/br2-external/board/raspberrypi/dos/AUTOEXEC.BAT" "$target/pi-dos/AUTOEXEC.BAT" <<'PY'
from pathlib import Path
import sys
source, destination = map(Path, sys.argv[1:])
destination.write_bytes(source.read_text().replace('\n', '\r\n').encode('ascii'))
PY
install -D -m 0644 "$project/br2-external/board/raspberrypi/dos/ARJ.EXE" \
    "$target/pi-dos/drivez/BIN/ARJ.EXE"
install -D -m 0644 "$project/br2-external/board/raspberrypi/dos/PIDOSCFG.EXE" \
    "$target/pi-dos/drivez/BIN/PIDOSCFG.EXE"
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
