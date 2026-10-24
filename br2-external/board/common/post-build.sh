#!/bin/sh
set -eu
target=$1
project="$BR2_EXTERNAL_ARMDOS_PATH/.."
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
install -D -m 0644 "$project/configs/armdos.conf" "$target/armdos/armdos.conf"
mkdir -p "$target/dosdata" "$target/run" "$target/tmp" "$target/proc" "$target/sys"
# No system shell, getty, login or init scripts are needed by the C supervisor.
test -x "$target/init"
test -x "$target/armdos/armdos"
test ! -e "$target/bin/sh"
test ! -e "$target/bin/busybox"
test ! -e "$target/sbin/init"
