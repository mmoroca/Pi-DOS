# Pi 5 Buildroot image

The image work starts from the official Buildroot 2026.08 Pi 5 board
configuration. Mouse validation and the known audio teardown XRUN are deferred
at the user's request. Their status is not changed by building this image.

The first image was generated on 21 October 2026; this report was updated on
22 October. The boot/keyboard/persistence results below refer to the earlier
ext4 image. The current FAT32 image has passed static and host resize checks
but still needs physical boot and shutdown-cursor retesting. Adding the D0 overlay resolved the reported kernel panic: the test
Pi now reaches the Pi-DOS prompt. After the input notification correction,
the user confirmed that the official wired Raspberry Pi USB keyboard works,
including after unplugging and reconnecting. The user subsequently reported
that the C: persistence, reboot and poweroff checks all passed.
The ReplayOS Pi was not modified.

| Artifact | Result |
| --- | --- |
| SD image | 806,354,944 bytes / 769 MiB |
| Kernel with embedded initramfs | 49,637,888 bytes |
| Uncompressed root filesystem CPIO | 45,184,000 bytes |
| Kernel release | `6.12.61-armdos-pi5`, 4 KiB pages |
| Current FAT32 image SHA-256 | `43a2bdf4b2ea4399f1e10c58f24a5ec51930d1f9dfb83089cd9e6ec4ac5d92ee` |

The actual FAT image contains the kernel, both Pi 5 DTBs, KMS and D0 overlays, and boot
configuration. DOSDATA is now FAT32 for direct file access from modern Windows
systems. Readback checks confirm the `c` and `d` directories and system batch
files, including the reboot request followed by `EXIT`.

## Build locally

```sh
make image-config
JOBS=4 make image
```

The host needs the normal Buildroot prerequisites: a C/C++ compiler, GNU Make,
Bash, Perl, Python 3.12+, patch, file, cpio, rsync, unzip, wget, bc, and the host
development tools required by Buildroot's dependency check. Downloads and build
outputs stay inside the project. The build must run as an ordinary user.

`make image-config` applies the tracked Pi 5 defconfig. `make image` configures
it automatically if missing, then resumes the existing build. It downloads the
pinned external AArch64 cross-toolchain and package archives as needed; first
builds take substantially longer than subsequent builds.
Existing configurations are refreshed with `olddefconfig` so changes to package
dependencies are incorporated without resetting local selections.

| Path | Purpose |
| --- | --- |
| `br2-external/` | External board configuration and packages; no Buildroot fork |
| `src/init/init.c` | Custom root PID 1 supervisor |
| `configs/armdos.conf` | Appliance DOS runtime defaults |
| `output/src/buildroot-2026.08/` | Verified, extracted upstream Buildroot |
| `dl/buildroot/` | Buildroot-managed package download cache |
| `output/buildroot-pi5/` | Isolated AArch64 cross-build output |
| `output/logs/buildroot-pi5.log` | Current build log |
| `output/buildroot-pi5/images/armdos-pi5-sdcard.img` | Generated SD card image |
| `output/buildroot-pi5/images/armdos-image-validation.json` | Static image checks and identity |

Buildroot's selected release is pinned by SHA-256 in `sources.lock.json`.
Its package hashes and the upstream Pi board kernel hash are enforced with
`BR2_DOWNLOAD_FORCE_CHECK_HASHES`. The DOS runtime uses the existing locked
DOSBox-X revision and separately maintained Pi-DOS display-dimension patch.
Buildroot supplies native SDL2 2.32.10 from its signed-release archive rather
than the Phase 0 codeload archive; both use the same SDL2 release version.

The initial image also inherits Buildroot's pinned Mesa and external Bootlin
toolchain. `BR2_REPRODUCIBLE` is enabled, but this is not a claim of verified
bit-for-bit reproducibility or completed distribution-license compliance.

## System layout

The Pi 5 EEPROM bootloader loads a FAT32 BOOT partition containing `Image`,
Pi 5 device trees, the KMS overlay, `config.txt` and `cmdline.txt`. The kernel
contains the entire Buildroot root filesystem as an embedded initramfs.
There is no separate Linux root partition.

| Partition | Format / initial size | Contents |
| --- | --- | --- |
| 1, BOOT | FAT32 / 256 MiB | ARM64 kernel with initramfs, device trees and boot configuration |
| 2, DOSDATA | FAT32 / initially 512 MiB | Expands on first boot; `c/`, `d/`, DOS system batch files and user data |

The first partition starts at 1 MiB. The fixed MBR signature is `a4d05001`.
The image targets Pi 5 only; Pi 3 and Pi 4 images are not implemented here.

### Custom DAC EDID

The image embeds `dac-crt.bin` at `/lib/firmware/edid/dac-crt.bin` in its
initramfs. `cmdline.txt` assigns it to HDMI-A-1 and HDMI-A-2 using
`drm.edid_firmware`; the kernel fragment explicitly enables EDID firmware
loading. The existing 720x400 at 70 Hz initial-mode requests remain in place.

The EDID preserves every timing and capability from `dac-original.bin` and
adds progressive 640x400 and 640x350 at approximately 70.100 Hz, plus a custom
VGA-compatible 720x348 at approximately 70.082 Hz and 640x350 at 60.000 Hz.
The original stereo LPCM audio block is retained. The generator promotes the
digital base EDID from 1.2 to 1.3 and clears reserved digital-input bits:
Linux skips HDMI/audio capability parsing below 1.3, causing VC4 to treat the
adapter as DVI and refuse audio even though its CTA audio block is present.
The 60 Hz mode retains a VGA-compatible 31.5 kHz horizontal scan with a
525-line total; it has a larger vertical blanking interval. The 720x348 mode adapts a
Hercules-sized image to VGA scan timing; it is not original Hercules timing.
The source EDID's remaining metadata/conformity issues are retained; see
`dac-crt.txt`. Recreate the binary with:

```sh
python3 tools/image/create-crt-edid.py dac-original.bin dac-crt.bin
```

Run `make image` after changing the EDID. Static image verification checks
the packed EDID against the project binary, its checksums, the kernel EDID
loader setting and the override parameter read back from the SD image.
Physical CRT lock and DOSBox-X mode selection still require hardware checks.

For an existing FAT32 Pi-DOS card, copy `output/buildroot-pi5/images/Image`
to BOOT as `Image` and `br2-external/board/pi5/cmdline.txt` to BOOT as
`cmdline.txt`, then safely eject and boot. Both files are needed for this
update; DOSDATA can be retained. The full regenerated SD image is also
available for a fresh flash. Placing the EDID binary on BOOT alone does not
make it available to the kernel's firmware loader.
Pi 5's EEPROM supplies its boot firmware; this image does not update EEPROM.

The DOS prompt starts in VGA 80x25 text mode with 9x16 character cells, giving
a native 720x400 raster at 70 Hz. BOOT's single-line `cmdline.txt` requests
`video=HDMI-A-1:720x400@70 video=HDMI-A-2:720x400@70` for either HDMI port.
The runtime uses `fullresolution=original`, `char9=true`,
`aspect=false`, `scaler=none` and SDL2 `modeswitch=true` to request that video
mode without software scaling or aspect correction, retaining the boot display's
70 Hz refresh rate for the prompt. The KMS surface patch requests the rendered
raster, including any horizontal pixel duplication, and selects an exact-size
HDMI mode at the advertised refresh nearest to the emulated video's rate.
DOS applications can still
select other video modes. The physical HDMI resolution depends on the modes
advertised by the connected display; a monitor without 720x400 at 70 Hz support may use
a different mode selected by SDL. Early kernel diagnostics remain on serial.
The runtime defaults are embedded in `Image`. To update an existing FAT32
installation, copy the rebuilt `output/buildroot-pi5/images/Image` and
`br2-external/board/pi5/cmdline.txt` onto BOOT. Physical 720x400 at 70 Hz output
still needs confirmation on the Pi and connected display.

Video diagnostics are disabled by default with `videodiagnostics=false` in
the `[sdl]` section of `configs/armdos.conf`. To enable them, set:

```ini
[sdl]
videodiagnostics=true
```

This is a startup setting. Rebuild `Image` after changing the embedded
configuration and restart the emulator. With diagnostics enabled, the log is
saved in `C:\SYSTEM\VIDEO.LOG` on writable DOSDATA and replaced at each
emulator start. An explicit `[log] logfile` setting overrides that path.
With diagnostics disabled and no explicit logfile, the video log is neither
created nor updated; verbose mode tracing and presentation queries are skipped.
Mode selection works identically with diagnostics enabled or disabled.
Look for `Pi-DOS video request`
and `Pi-DOS video presented`: these include guest raster/refresh, rendered
dimensions, the selected host mode, surface size and renderer output size.
The available host modes are also listed, making an EDID override that did
not load visible without a Linux shell. If the file cannot be written, the
emulator still starts and serial diagnostics remain available.

A headless check of the freely distributed Pinball Fantasies demo's High
mode measured a 320x350 VGA raster at approximately 70.399 Hz and a 640x350
rendered surface with `doublescan=true`, `aspect=false` and `scaler=none`.
That version did not generate 700 active lines. This does not establish the
mode used by every release or confirm CRT geometry on the Pi. Retest the
gameplay screen and inspect `VIDEO.LOG` before adding speculative modes.
The user subsequently confirmed that the native output looks correct on a PC
CRT; the LCD test monitor introduced a separate scaling/sampling concern.

The first kernel is Raspberry Pi Linux 6.12.61 at the upstream board's pinned
revision `21b410140c47ffab5668399f6f143c7d7b935c8b`, using the `bcm2712` baseline
plus 4 KiB pages and an Pi-DOS fragment. The release suffix is `-armdos-pi5`;
static validation checks `CONFIG_ARM64_4K_PAGES=y` directly.
Display, storage, USB, input and sound drivers are built in,
with no module loader. The IP stacks, network interfaces, Wi-Fi, Bluetooth and
KVM are disabled. Camera/TV, staging drivers, IIO and unrelated general-purpose
filesystems are also excluded, since the distribution defconfig's optional
modules otherwise become built-ins when module support is disabled.
Netlink/Unix facilities remain for daemon-free device discovery. Further
hardware-driver trimming follows a working board baseline.

Userspace includes native SDL2 KMS/DRM, ALSA, Mesa VC4/V3D, GBM/EGL/GLES and
`libudev-zero`. SDL2's upstream Buildroot recipe normally checks for a udev
daemon provider; `external.mk` enables the libudev API and its dependency for
`libudev-zero` instead. No device-management daemon runs.
The runtime package declares libpng and ALSA sequencer APIs required by the
selected engine sources. MIDI remains disabled in the default DOS configuration.

No BusyBox, Linux shell, login/getty, SSH server, desktop or general-purpose
init system is selected. The default command line sends diagnostics to serial;
HDMI boot diagnostics can be enabled temporarily as described below.
The appliance is not remotely administered by SSH.

The first generated image logged only to serial. Following a black-screen
report, a diagnostic command line enabled the HDMI console as well. This
is a diagnostics change, not a confirmed fix for the reported boot failure. That
diagnostic image passed static validation with SHA-256
`be76fd59867001c05d8d42e66822eee8240ac842a8a95937fd8bf466ab5f80f3`.
The subsequent D0 correction also requires the overlay file described below.

The subsequent HDMI photo exposed an early kernel panic in
`bcm2712_pull_config_set`, reached through `brcmuart_init`, before `/init` runs.
The original image omitted `overlays/bcm2712d0.dtbo`. Older EEPROM firmware
uses this overlay to adapt the base DTB for D0 silicon; providing the separate
`bcm2712d0-rpi-5-b.dtb` alone does not cover that boot path. The build now
packages both forms and verifies all four DTB/overlay files by reading them
back from the generated SD image. The user confirmed that adding the overlay
gets the test Pi to the Pi-DOS prompt.

## PID 1, storage and DOS startup

`/init` checks that it is root PID 1 before doing anything. It mounts devtmpfs,
procfs, sysfs, devpts and temporary memory-backed directories. It then waits up
to ten seconds for the data device from `armdos.data=` in the kernel command
line (default `/dev/mmcblk0p2`). It verifies a valid FAT32 boot sector and the
DOSDATA label before mounting. It automatically expands DOSDATA when unused
card capacity is available, as described below; it never formats a device.
The current image deliberately refuses an old ext4 DOSDATA partition.

Successful writable storage supplies C: from `/dosdata/c` and D: from
`/dosdata/d`. If a writable mount fails, init attempts a read-only mount with
VFAT read-only options, and presents the drives read-only if usable.
Missing, wrongly labelled or unmountable storage produces a DOS Z: recovery
prompt, with no temporary replacement C: drive and no infinite device wait.
That prompt does not include a filesystem-repair utility.

The image seeds `C:\SYSTEM\EDIT.COM` with the personalized PEDIT Light binary
from `br2-external/board/pi5/dos/EDIT.COM`. The default PATH selects this copy
before `Z:\DOS\EDIT.COM`. PEDIT stores its settings inside the executable, so
further preferences saved through Alt+F1 persist on writable DOSDATA. Rebuilding
the image restores the seed preferences; updating only BOOT does not replace an
existing editor on DOSDATA. Static image verification compares the packaged
editor byte-for-byte with the seed.

### Automatic FAT32 DOSDATA expansion

On first boot, PID 1 grows partition 2 and its FAT32 filesystem to the remaining
SD-card capacity before mounting C: and D:. Both drives share this space. The
image keeps an initial 512 MiB filesystem with 4 KiB clusters, MBR type `0x0c`,
and the correct hidden-sector offset. Modern Windows can open DOSDATA directly:
its `c` directory supplies C: and `d` supplies D:.

Expansion requires the Pi-DOS MBR identity, 512-byte sectors, the original
BOOT partition and DOSDATA start, a valid FAT32 DOSDATA boot sector, and no
additional partitions. Other layouts are left unchanged. Cards outside the
current MBR addressing range are not automatically expanded.

The supervisor runs the non-writing `fsck.fat -n` check only when growth is
needed. It uses libfdisk to change DOSDATA's size and refresh the kernel
partition table, then uses libparted's FAT resizer while DOSDATA is unmounted.
A second FAT check verifies the result. It preserves BOOT, DOSDATA's starting
position and existing files, including programs copied onto the seed partition
before first boot. The resizer may leave less than one cluster of padding at
the end of the partition. The library is explicitly instructed to retain FAT32
on small volumes; it does not convert them to FAT16.

Two compatibility patches are maintained for pinned Parted 3.6: preserve the
original FAT32 BPB volume label during resize, and initialize its Atari-label
locale outside a disabled debug assertion (which otherwise crashes library
clients on exit). They are applied to host and target builds.

Every boot compares partition and filesystem geometry. Once full, it skips the
tools and partition writes. A later boot can also finish filesystem growth if
partition growth completed earlier. This is not a guarantee of recovery from
power loss during a FAT update. FAT32 is not journaled: use `POWEROFF` before
removing the card and safely eject it after copying files from Windows.
A failed FAT check, resize error or five-minute operation timeout triggers the
existing read-only/recovery path. No failed expansion causes reformatting.

The production FAT32 resize routine passed native host tests against sparse
2 GiB and 64 GiB card images. Both kept FAT32 and the DOSDATA label, preserved
files in `c` and `d` and BOOT data, passed `fsck.fat -n`, and skipped repeat
growth. The driver is `tools/image/test-fat32-file.py`, with
`tests/boot/resize-fat-image.c`; evidence is in
`output/logs/fat32-file-validation.log`. This exercises filesystem resizing
and data preservation on regular files. The ARM64 loop-device driver is
`tools/image/test-expand.py` with `tests/boot/expand-data.c`; it additionally
checks kernel partition refresh, VFAT mounting and refusal of foreign layouts.
The development Pi was unreachable over SSH during this build, so the new
ARM64 loop test and physical SD-card test are still pending. Earlier ext4
loop-test results do not validate this FAT32 version.

**Migration from the previous image requires a new flash.** Back up the old
DOSDATA `c` and `d` directories, write the new SD image, then restore those
files into the new FAT32 directories. Replacing only BOOT's `Image` will not
convert ext4 into FAT32. No physical card is flashed by the build tools.
Allow first boot extra time to finish expansion before the DOS prompt.

Init generates runtime startup commands under `/run`, selects a connected HDMI
DRM card (up to five seconds for discovery) and fixes audio to the primary HDMI0
port (HDMI-A-1, the micro-HDMI connector nearest USB-C power). SDL uses
`AUDIODEV=plughw:CARD=vc4hdmi0,DEV=0`, independent of ALSA card enumeration or
EDID audio detection. Audio does not fall back to HDMI1 or another sound card;
connect the HDMI-to-VGA adapter with its audio jack to HDMI0.
It requests native KMS/DRM, GLES and ALSA, attempts a
read-only remount of the volatile system root, then launches the DOS runtime as
a supervised child. A failed read-only remount is logged; the system image on
the SD card is not used as a writable Linux root filesystem.

The generated DOS environment uses the ARM64 dynrec core, US keyboard layout,
SB16 audio defaults and fixed HDMI output selected by SDL. Its default PATH is:

```dos
PATH C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\DEBUG;Z:\PATCHING;Z:\4DOS
```

C:\SYSTEM stays first for the Pi-DOS lifecycle commands. If
`C:\AUTOEXEC.BAT` exists, it runs after the system startup commands and can
override PATH.

Startup displays a BIOS-style configuration table followed by
`Welcome to Pi-DOS`. Board and Cortex processor names come from the live device
tree; CPU cores are the online core count. Host RAM is kernel-usable memory
(not the advertised installed capacity), displayed in binary units. Storage
identifies the selected DOSDATA block device, with mounted filesystem capacity,
available space and access mode. C: and D: share that space; only available
drive mappings are listed. DOS video, memory, audio and keyboard fields are read
from the runtime configuration, and the Pi-DOS version comes from the init
package build version. Unavailable readings show `Unknown` or `Unavailable`.
These describe configured emulated devices, not physical HDMI/audio detection.
The table fits the native 80x25 screen without a delay or keypress. Read-only
and recovery warnings still appear below it, before the user's AUTOEXEC.BAT.

### Host cursor during shutdown

The user reported that the first DOSBox-X/SDL2 cleanup changes still allowed
the desktop arrow to flash at the top left during poweroff. The current fix
sets the local `SDL_KMSDRM_HIDE_CURSOR=1` hint in PID 1. SDL2's KMS/DRM backend
enforces this hint for every cursor-show request, including mouse focus and
capture changes, and clears the cursor plane when creating or destroying its
buffer. DOS applications still render their own emulated cursors; the
upstream `usesystemcursor` setting defaults to false and should remain false
on this appliance. The hint is an Pi-DOS extension, not an upstream SDL2 API.

For this retest, replace only `Image` on the card's BOOT partition with
`output/buildroot-pi5/images/Image`, then safely eject, boot and execute
`POWEROFF`. Also check `REBOOT`. Both SDL2 and PID 1 are embedded in the kernel;
DOSDATA does not need to be rewritten. Visual confirmation on the test board
is still required.

## Shutdown protocol

`EXIT` or `POWEROFF` exits the DOS shell. The runtime cleans up before returning
to PID 1; PID 1 syncs, unmounts DOSDATA and requests poweroff. `REBOOT` writes
the exact word `reboot` to `C:\SYSTEM\CONTROL.REQ` and exits the shell. PID 1
reads and removes that request after the runtime exits, then syncs/unmounts and
requests reboot. A stale request is removed on the next writable boot.

The reboot batch file is available on the initial writable C: image. Recovery
mode supports `EXIT`; it does not promise a DOS-issued host reboot when C: is
missing or read-only. PID 1 also handles SIGINT as reboot and SIGTERM as poweroff,
allowing ten seconds for a supervised runtime to stop before forcing termination.
Unexpected runtime crashes preserve serial diagnostics without a restart loop.

The user confirmed successful `REBOOT`, persistence of the test file after
restart, and `POWEROFF` on the appliance. This confirms the requested functional
checks; it does not establish power-loss recovery or the deferred ALSA playback
teardown behavior.

## Validation and first hardware test

The build runs `tools/image/verify.py` after image generation. It checks the
actual initramfs, custom AArch64 init/runtime, native SDL2 backends, ARM64 dynrec,
required built-in kernel drivers, 4 KiB pages, device trees, partition types and
DOSDATA label. It rejects an included Linux shell, BusyBox or conventional init.
It writes hashes and the exact source lock beside the image.
It also checks ELF architecture and linked-library presence throughout the
target root filesystem, rejecting library search paths pointing into the host
build tree.

Static checks do not establish that the image boots on a Pi. The first hardware
gate is cold boot from a separate test SD card, observe the DOS prompt through
HDMI or serial diagnostics, write a C: file, use `REBOOT`, and confirm the file
survives. Then use `POWEROFF` and verify clean storage shutdown.

On 22 October 2026, the user reported that all requested hardware checks passed:

- Boot to the Pi-DOS prompt on the test Pi 5.
- Physical wired USB keyboard input, including unplugging and reconnecting.
- Create and read `C:\BOOTTEST.TXT`.
- Run `REBOOT` and read the same file after restart.
- Run `POWEROFF` successfully.

These are user-reported functional results on the test board. Mouse validation
and the audio teardown XRUN remain deferred. The reported cursor flash
during reboot/poweroff is addressed by the KMS/DRM patches above and awaits
visual retesting. `NOTES.md` was not edited for this change.

The current ReplayOS development Pi has SSH-only access. This work builds an
image locally and does not replace its SD card, reboot it or modify its boot
configuration. Testing this appliance image requires a spare boot medium and
an observation path because the appliance deliberately contains no SSH server.

### Black screen on the first image

The original image's command line sends diagnostics only to serial. A lit
black display therefore cannot distinguish an early boot problem from a DOS
runtime/display failure. On a computer, open the SD card's FAT32 BOOT partition
and replace `cmdline.txt` with the following **single line**:

```text
console=serial0,115200 console=tty1 rdinit=/init loglevel=7 ignore_loglevel vt.global_cursor_default=0 armdos.data=/dev/mmcblk0p2
```

This changes only boot diagnostics and needs no kernel rebuild
or SD-card reflash. To keep the 720x400 at 70 Hz request during diagnostics,
append `video=HDMI-A-1:720x400@70 video=HDMI-A-2:720x400@70` to that same line.
Boot again with HDMI attached and record the last visible
messages, especially `Kernel panic`, `Pi-DOS:` or SDL/Mesa errors. If the
display remains black without any text, serial diagnostics are the next way
to separate an early boot failure from a display-console failure.

After boot and keyboard operation were confirmed, the tracked `cmdline.txt`
was restored to serial-only diagnostics at the user's request. The current
configuration also requests 720x400 at 70 Hz. To restore an existing card,
copy `br2-external/board/pi5/cmdline.txt` to BOOT, or use this single line:

```text
console=serial0,115200 rdinit=/init loglevel=4 vt.global_cursor_default=0 armdos.data=/dev/mmcblk0p2 video=HDMI-A-1:720x400@70 video=HDMI-A-2:720x400@70 drm.edid_firmware=HDMI-A-1:edid/dac-crt.bin,HDMI-A-2:edid/dac-crt.bin
```

This needs no rebuild or reflash. Keep the corrected `Image` and D0 overlay.

### UART pinctrl panic on D0 boards

The original panic ends with `Asynchronous SError Interrupt` and includes
`bcm2712_pull_config_set`, `bcm2712_pinconf_set` and `brcmuart_init`. This matches
[an upstream report resolved by supplying the D0 overlay](https://github.com/agherzan/meta-raspberrypi/issues/1394).
The pinned kernel includes the overlay source; the image packaging omitted it.

For the immediate retest, copy the newly generated
`output/buildroot-pi5/images/bcm2712d0.dtbo` into the existing SD card's
`overlays/` directory on its FAT32 BOOT partition. Keep the diagnostic
`cmdline.txt` above. Safely eject the card and boot again.
No full reflash, kernel replacement or DOSDATA modification is needed.
Leave `config.txt` unchanged: the firmware selects the silicon-specific
description; do not force the D0 overlay on every Pi 5.

Future image builds include the file automatically. The user confirmed that
this change reaches the DOS prompt. Keyboard input and reconnection were also
confirmed after the PID 1 update below.

The corrected image passed static validation, including readback of the D0
overlay from its BOOT partition. Its SHA-256 is
`a3f1e209d06d497059cd71cbb661df8e7c1c1337f1f8bae3112a0e1f7f72b5fc`.

### DOS prompt appears but USB keyboard does not respond

The first successful boot had no input from an official wired Raspberry Pi
keyboard. Reconnecting it did not help. The kernel includes USB host, HID and
evdev support, but the original PID 1 omitted the notification forwarding
required by libudev-zero. SDL's initial device scan can miss a keyboard whose
USB enumeration finishes later; reconnecting also requires those notifications.

PID 1 now subscribes to kernel device events before starting SDL and forwards
input/sound events to libudev-zero's userspace netlink group. devtmpfs provides
the device nodes, and libudev-zero classifies input from sysfs. This implements
the forwarding described in the library's
[hotplug documentation](https://github.com/illiliti/libudev-zero/tree/1.0.5#hotplugging)
without adding a daemon or changing device permissions.

A focused SSH check on the existing development Pi used the actual PID 1
forwarding functions and the Buildroot-built libudev-zero library. A temporary
uinput keyboard created after the initial scan was received through the bridge
and classified with `ID_INPUT_KEY=1` at `/dev/input/event11`. The temporary
device and files were removed. No keystrokes were injected, and no ReplayOS
services were changed. This check establishes late device discovery; it does
not establish physical USB keyboard operation in the appliance.

For the immediate hardware retest, replace only `Image` on the card's BOOT
partition with `output/buildroot-pi5/images/Image`. The initramfs and updated
PID 1 are embedded in that kernel. Keep the D0 overlay and diagnostic boot
configuration already on the card. Safely eject, boot with the keyboard
connected, and try `DIR` followed by Enter; also check reconnecting the keyboard.
DOSDATA does not need to be rewritten.

The input-correction image passed static validation. Its SHA-256 is
`41f1a41889d0123b0b7a61f62ec54cf2a8b2205eeaf9e5567183fdc4dd579f92`.

On 22 October 2026, the user confirmed that the physical keyboard works on the
appliance, including after unplugging and reconnecting. This hardware retest
confirms the correction for the reported keyboard failure. The subsequent
persistence/reboot/poweroff checks also passed, as recorded above. Mouse
validation remains deferred.

## References

- [Official Buildroot 2026.08 release](https://buildroot.net/download.html)
- [Buildroot manual: external customization and initramfs](https://buildroot.buildroot.org/downloads/manual/manual.html)
- The pinned release's `configs/raspberrypi5_defconfig`, `package/sdl2/`,
  `package/libudev-zero/` and `board/raspberrypi/patches/linux/linux.hash`.

Original Pi-DOS code licensing is still undecided. Complete source/license
inventory and required distribution materials before publishing an image.
