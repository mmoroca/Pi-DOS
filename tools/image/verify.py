#!/usr/bin/env python3
"""Check generated Pi 5 image structure; this is not a hardware boot test."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'output/buildroot-pi5'


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    images = OUT / 'images'
    target = OUT / 'target'
    for name in ('init', 'armdos/armdos'):
        data = (target / name).read_bytes()[:20]
        require(data[:5] == b'\x7fELF\x02' and struct.unpack_from('<H', data, 18)[0] == 183,
                f'{name} is not ELF64 AArch64')
    with (images / 'rootfs.cpio').open('rb') as stream:
        names = subprocess.check_output(['cpio', '-it', '--quiet'], stdin=stream, text=True).splitlines()
    names = {name.removeprefix('./') for name in names}
    for name in ('init', 'armdos/armdos', 'armdos/armdos.conf', 'dev/console',
                 'sbin/fsck.fat', 'lib/firmware/edid/dac-crt.bin'):
        require(name in names, f'Missing initramfs file: {name}')
    with (images / 'rootfs.cpio').open('rb') as stream:
        packed_init = subprocess.check_output(['cpio', '-i', '--to-stdout', '--quiet', 'init'], stdin=stream)
    require(packed_init == (target / 'init').read_bytes(), 'Stale PID 1 in initramfs')
    with (images / 'rootfs.cpio').open('rb') as stream:
        packed_edid = subprocess.check_output([
            'cpio', '-i', '--to-stdout', '--quiet', 'lib/firmware/edid/dac-crt.bin'], stdin=stream)
    require(packed_edid == (ROOT / 'dac-crt.bin').read_bytes(), 'Missing/stale CRT EDID in initramfs')
    require(len(packed_edid) >= 128 and len(packed_edid) == (packed_edid[126] + 1) * 128
            and packed_edid[:8] == bytes.fromhex('00 ff ff ff ff ff ff 00')
            and all(sum(packed_edid[i:i + 128]) % 256 == 0
                    for i in range(0, len(packed_edid), 128)), 'Invalid packed CRT EDID')
    require(packed_edid[18] == 1 and packed_edid[19] >= 3 and packed_edid[20] & 0x80,
            'CRT EDID must be digital EDID 1.3+ for Linux HDMI/audio capability parsing')
    default_path = r'C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\DEBUG;Z:\PATCHING;Z:\4DOS'
    require(('SET PATH=' + default_path).encode() in packed_init, 'Default DOS PATH is incomplete')
    for name in ('bin/sh', 'bin/busybox', 'sbin/init', 'usr/bin/bash', 'usr/sbin/sshd',
                 'usr/sbin/dropbear', 'sbin/e2fsck', 'sbin/resize2fs'):
        require(name not in names, f'Unexpected general-purpose runtime: {name}')
    basenames = {Path(name).name for name in names}
    # Catch host contamination and missing linked libraries before hardware boot.
    readelf = OUT / 'host/bin/aarch64-linux-readelf'
    for path in target.rglob('*'):
        if path.is_symlink() or not path.is_file():
            continue
        with path.open('rb') as stream:
            header = stream.read(20)
        if header[:4] != b'\x7fELF':
            continue
        require(header[4] == 2 and struct.unpack_from('<H', header, 18)[0] == 183,
                f'Non-AArch64 ELF in target: {path.relative_to(target)}')
        dynamic = subprocess.check_output([str(readelf), '-d', str(path)], text=True)
        if path == target / 'init':
            require('libfdisk.so' in dynamic, 'PID 1 lacks DOSDATA growth support')
            require('libparted-fs-resize.so' in dynamic, 'PID 1 lacks FAT32 growth support')
        for library in re.findall(r'\(NEEDED\).*\[(.*?)\]', dynamic):
            require(library in basenames, f'{path.name} requires missing library {library}')
        require(not re.search(r'\((?:RUNPATH|RPATH)\).*\[.*(?:/home/|/output/)', dynamic),
                f'Host build path in {path.name} library search path')
    require(any('libSDL2-2.0.so' in name for name in names), 'SDL2 library missing')
    require(any(name.endswith('/ld-linux-aarch64.so.1') for name in names), 'AArch64 dynamic loader missing')
    require(not any('libSDL3' in name for name in names), 'Unexpected SDL3 library')
    # Mesa 26 installs a combined libgallium instead of per-driver DRI symlinks.
    require(any('/libgallium-' in name and name.endswith('.so') for name in names), 'Mesa Gallium library missing')
    mesa_options = next((OUT / 'build').glob('mesa3d-*/buildroot-build/meson-info/intro-buildoptions.json'))
    drivers = next(option['value'] for option in json.loads(mesa_options.read_text())
                   if option['name'] == 'gallium-drivers')
    require('v3d' in drivers and 'vc4' in drivers, 'Mesa lacks Pi VC4/V3D drivers')
    require(any('libudev.so' in name for name in names), 'Daemon-free libudev missing')
    require(any('libfdisk.so' in name for name in names), 'DOSDATA expansion library missing')
    require(any('libparted-fs-resize.so' in name for name in names), 'FAT32 resizer missing')
    linux_dirs = list((OUT / 'build').glob('linux-*/.config'))
    require(len(linux_dirs) == 1, 'Cannot identify kernel build configuration')
    kernel = linux_dirs[0].read_text()
    for setting in ('ARM64_4K_PAGES', 'DEVTMPFS', 'VFAT_FS', 'INPUT_EVDEV',
                    'SIGNALFD', 'BINFMT_ELF', 'FUTEX', 'PROC_FS', 'SYSFS', 'TMPFS', 'UNIX98_PTYS',
                    'USB_HID', 'USB_DWC3', 'MMC_SDHCI_BRCMSTB', 'PCIE_BRCMSTB',
                    'MFD_RP1', 'PINCTRL_RP1', 'DRM_VC4', 'DRM_V3D', 'DRM_LOAD_EDID_FIRMWARE', 'SND_SOC'):
        require(f'CONFIG_{setting}=y\n' in kernel, f'Required kernel option not built in: {setting}')
    for setting in ('INET', 'IPV6', 'MODULES', 'MEDIA_SUPPORT', 'STAGING', 'BTRFS_FS', 'BCACHEFS_FS'):
        require(f'CONFIG_{setting}=y\n' not in kernel, f'Unexpected kernel option: {setting}')
    config = next((OUT / 'build').glob('armdos-runtime-*/config.h')).read_text()
    for setting in ('C_SDL2', 'C_DYNREC'):
        require(f'#define {setting} 1' in config, f'Runtime missing {setting}')
    require('#define C_SDL3 1' not in config, 'Runtime unexpectedly uses SDL3')
    require('#define C_TARGETCPU ARMV8LE' in config, 'Runtime lacks ARM64 CPU backend')
    sdl = next((OUT / 'build').glob('sdl2-*/include/SDL_config.h')).read_text()
    for setting in ('HAVE_LIBUDEV_H', 'SDL_INPUT_LINUXEV', 'SDL_VIDEO_DRIVER_KMSDRM',
                    'SDL_VIDEO_OPENGL_ES2', 'SDL_AUDIO_DRIVER_ALSA'):
        require(f'#define {setting} 1' in sdl, f'SDL2 missing required backend: {setting}')
    with (images / 'Image').open('rb') as stream:
        header = stream.read(64)
    require(header[56:60] == b'ARMd', 'Missing ARM64 kernel Image header')
    require('rootfs.cpio' in kernel, 'Kernel is not configured for the embedded initramfs')
    trees = ('bcm2712-rpi-5-b.dtb', 'bcm2712d0-rpi-5-b.dtb',
             'vc4-kms-v3d-pi5.dtbo', 'bcm2712d0.dtbo')
    for name in trees:
        require((images / name).read_bytes()[:4] == b'\xd0\x0d\xfe\xed', f'Invalid device tree: {name}')
    disk = images / 'armdos-pi5-sdcard.img'
    with disk.open('rb') as stream:
        mbr = stream.read(512)
        require(mbr[510:] == b'\x55\xaa', 'Invalid MBR signature')
        require(struct.unpack_from('<I', mbr, 440)[0] == 0xa4d05001, 'Unexpected MBR disk identifier')
        partitions = []
        for index, expected_type in enumerate((0x0c, 0x0c)):
            entry = mbr[446 + index*16:462 + index*16]
            start, sectors = struct.unpack_from('<II', entry, 8)
            require(entry[4] == expected_type and start > 0 and sectors > 0, 'Invalid partition entry')
            require((start + sectors) * 512 <= disk.stat().st_size, 'Partition exceeds image')
            partitions.append({'type': expected_type, 'start_sector': start, 'sectors': sectors})
        stream.seek(partitions[0]['start_sector'] * 512)
        boot = stream.read(512)
        require(boot[82:90] == b'FAT32   ', 'BOOT partition is not FAT32')
        stream.seek(partitions[1]['start_sector'] * 512)
        data_boot = stream.read(512)
        require(data_boot[82:90] == b'FAT32   ' and data_boot[71:82] == b'DOSDATA    ',
                'DOSDATA is not labelled FAT32')
        require(struct.unpack_from('<I', data_boot, 28)[0] == partitions[1]['start_sector'],
                'DOSDATA hidden-sector offset is incorrect')
    mcopy = OUT / 'host/bin/mcopy'
    data_volume = f'{disk}@@{partitions[1]["start_sector"] * 512}'
    for name in ('c/README.TXT', 'c/SYSTEM/REBOOT.BAT', 'c/SYSTEM/POWEROFF.BAT'):
        require(subprocess.check_output([str(mcopy), '-i', data_volume, f'::{name}', '-']),
                f'DOSDATA missing {name}')
    editor = ROOT / 'br2-external/board/pi5/dos/EDIT.COM'
    packed_editor = subprocess.check_output([
        str(mcopy), '-i', data_volume, '::c/SYSTEM/EDIT.COM', '-'])
    require(packed_editor == editor.read_bytes(), 'Personalized EDIT.COM missing/stale in DOSDATA')
    require(subprocess.check_output([str(OUT / 'host/bin/mdir'), '-i', data_volume, '::d']),
            'DOSDATA missing directory d')
    # Check the actual SD image, not only loose build outputs: a missing D0
    # overlay can panic older EEPROM bootloaders during UART pinctrl setup.
    packed_kernel = subprocess.check_output([
        str(mcopy), '-i', f'{disk}@@{partitions[0]["start_sector"] * 512}', '::Image', '-'])
    require(packed_kernel == (images / 'Image').read_bytes(), 'BOOT kernel Image missing/stale')
    for name in trees:
        boot_path = f'overlays/{name}' if name.endswith('.dtbo') else name
        packed = subprocess.check_output([
            str(OUT / 'host/bin/mcopy'), '-i',
            f'{disk}@@{partitions[0]["start_sector"] * 512}', f'::{boot_path}', '-'])
        require(packed == (images / name).read_bytes(), f'BOOT device tree missing/stale: {boot_path}')
    for name in ('cmdline.txt', 'config.txt'):
        packed = subprocess.check_output([
            str(mcopy), '-i', f'{disk}@@{partitions[0]["start_sector"] * 512}', f'::{name}', '-'])
        require(packed == (ROOT / 'br2-external/board/pi5' / name).read_bytes(),
                f'BOOT configuration missing/stale: {name}')
        if name == 'cmdline.txt':
            require(len(packed.decode().splitlines()) == 1, 'BOOT cmdline.txt must be a single line')
            require('drm.edid_firmware=HDMI-A-1:edid/dac-crt.bin,HDMI-A-2:edid/dac-crt.bin'
                    in packed.decode().split(), 'BOOT command line lacks the CRT EDID override')
    evidence = {'scope': 'Static image validation only; this rebuilt image requires hardware validation',
                'image': str(disk.relative_to(ROOT)), 'image_bytes': disk.stat().st_size,
                'image_sha256': digest(disk), 'partitions': partitions,
                'initramfs_bytes': (images / 'rootfs.cpio').stat().st_size,
                'kernel_image_bytes': (images / 'Image').stat().st_size,
                'mesa_gallium_drivers': drivers,
                'kernel_config_sha256': digest(linux_dirs[0]),
                'kernel_release': (linux_dirs[0].parent / 'include/config/kernel.release').read_text().strip(),
                'buildroot_config_sha256': digest(OUT / '.config'),
                'source_lock': json.loads((ROOT / 'sources.lock.json').read_text()),
                'init_source_sha256': digest(ROOT / 'src/init/init.c'),
                'storage_source_sha256': digest(ROOT / 'src/init/storage.c'),
                'editor_sha256': digest(editor),
                'crt_edid_sha256': digest(ROOT / 'dac-crt.bin'),
                'patch_sha256': {str(p.relative_to(ROOT)): digest(p)
                                 for p in sorted((ROOT / 'patches').rglob('*.patch'))},
                'deferred': ['audio teardown underrun', 'mouse validation']}
    (images / 'armdos-image-validation.json').write_text(json.dumps(evidence, indent=2) + '\n')
    print(f'PASS: AArch64 kernel/runtime, custom initramfs, FAT32 BOOT and DOSDATA: {disk}')
    print('Hardware boot of this image remains unverified; validation metadata saved beside the image.')


if __name__ == '__main__':
    try:
        main()
    except (OSError, RuntimeError, subprocess.SubprocessError, ValueError, StopIteration) as error:
        sys.exit(str(error))
