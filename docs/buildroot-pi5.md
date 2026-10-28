# Shared Raspberry Pi ARM64 Buildroot image

The default profile is now `br2-external/configs/pi_dos_arm64_defconfig`.
It builds one image for the BCM2837, BCM2711 and BCM2712 families using a
Cortex-A53/ARMv8-A instruction baseline and a 4 KiB-page `bcm2711` kernel.
Board assets live in `br2-external/board/raspberrypi`; this guide retains its
historical filename. The earlier Pi 5-only build directory is not reused.

| Models targeted by the shared image | Boot assets |
| --- | --- |
| Pi 2B rev 1.2; Pi 3B, 3B+, 3A+; Zero 2 W; CM0, CM3 and CM3+ | BCM2710 DTBs, `bootcode.bin`, `start.elf`, `fixup.dat`, VC4 KMS |
| Pi 4B, 400; CM4 and CM4S | BCM2711 DTBs, `start4.elf`, `fixup4.dat`, Pi 4 KMS |
| Pi 5, 500/500+; CM5 and CM5 Lite | BCM2712 DTBs, D0 compatibility overlay, Pi 5 KMS, EEPROM firmware |

The firmware chooses the board DTB; `config.txt` uses documented model filters
to select its KMS overlay. Pi 3A+ shares the Pi 3B+ firmware DTB selection;
500+ shares the Pi 500 family. Compute Modules require suitable carrier hardware
and SD/eMMC provisioning; Zero 2 W needs a USB host adapter for input. Wired
Ethernet provides DHCP and network time. Wi-Fi, DSI and composite display support
are outside this HDMI/USB appliance profile.
Pi 1, original Zero/Zero W and Pi 2B rev 1.1 cannot run this AArch64 image.

This is the intended board coverage, not a record of successful boots on every
board. The user confirmed working Pi 3/Pi 5 input and Pi 4 operation after the
firmware correction recorded below. Other models still require hardware testing.
Model filters and 64-bit eligibility follow the
[official Raspberry Pi configuration documentation](https://www.raspberrypi.com/documentation/computers/config_txt.html).

## Earlier Pi 5 validation

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
| Kernel release | `6.12.61-pi-dos-pi5`, 4 KiB pages |
| Earlier Pi 5 FAT32 image SHA-256 | `43a2bdf4b2ea4399f1e10c58f24a5ec51930d1f9dfb83089cd9e6ec4ac5d92ee` |

The shared FAT image contains the kernel, the device trees listed in
`br2-external/board/raspberrypi/device-trees.txt`, KMS/USB overlays, boot firmware
and configuration. PI-DOS is FAT32 for direct file access from modern Windows
systems. Readback checks confirm the `c` directory and system batch
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

`make image-config` applies the tracked shared ARM64 defconfig. `make image` configures
it automatically if missing, then resumes the existing build. It downloads the
pinned external AArch64 cross-toolchain and package archives as needed; first
builds take substantially longer than subsequent builds.
Existing configurations are refreshed with `olddefconfig` so changes to package
dependencies are incorporated without resetting local selections.

| Path | Purpose |
| --- | --- |
| `br2-external/` | External board configuration and packages; no Buildroot fork |
| `src/init/init.c` | Custom root PID 1 supervisor |
| `configs/pi-dos.conf` | Appliance DOS runtime defaults |
| `output/src/buildroot-2026.08/` | Verified, extracted upstream Buildroot |
| `dl/buildroot/` | Buildroot-managed package download cache |
| `output/buildroot-arm64/` | Isolated AArch64 cross-build output |
| `output/logs/buildroot-arm64.log` | Current build log |
| `output/buildroot-arm64/images/pi-dos-arm64-sdcard.img` | Generated SD card image |
| `output/buildroot-arm64/images/pi-dos-image-validation.json` | Static image checks and identity |

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

The board's firmware loads a FAT32 BOOT partition containing `Image`,
model device trees, KMS/USB overlays, `config.txt` and `cmdline.txt`. The kernel
contains the entire Buildroot root filesystem as an embedded initramfs.
There is no separate Linux root partition.

| Partition | Format / initial size | Contents |
| --- | --- | --- |
| 1, BOOT | FAT32 / 256 MiB | ARM64 kernel with initramfs, device trees and boot configuration |
| 2, PI-DOS | FAT32 / initially 512 MiB | Expands on first boot; `c/`, DOS system batch files and user data |

The first partition starts at 1 MiB. The fixed MBR signature is `a4d05001`.
All targeted models use this same partition layout and data volume.

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

For an existing FAT32 Pi-DOS card, copy `output/buildroot-arm64/images/Image`
to BOOT as `Image` and `br2-external/board/raspberrypi/cmdline.txt` to BOOT as
`cmdline.txt`, then safely eject and boot. Both files are needed for this
update; PI-DOS can be retained. The full regenerated SD image is also
available for a fresh flash. Placing the EDID binary on BOOT alone does not
make it available to the kernel's firmware loader.
Pi 5's EEPROM supplies its boot firmware; this image does not update EEPROM.

### Monitor profiles

PIDOSCFG's System page selects LCD (the fresh-install default) or CRT. LCD
uses a manually selected fixed output: 640x480, 800x600, 832x624, **1024x768**
(default), 1280x720 or 1920x1080. These resolutions come from the supplied EDID,
which remains installed for both profiles. No physical-monitor detection is
added. SDL selects an advertised refresh for the chosen resolution independently
of the guest's refresh; the mode remains fixed across guest changes. A closest
available resolution fallback is recorded when video diagnostics are enabled.

LCD aspect is 4:3 by default, or Original pixels (the uncorrected source raster
ratio). A separate source surface is fitted into the fixed window, centered with
black borders. This also handles DOS modes larger than the selected output.
VGA doublescan and software scalers are disabled for this profile. Final
nearest-neighbor fitting runs on the CPU; SDL's existing GLES presentation
and VSync remain enabled.
No integer-scaling control or forced-refresh option is added.

The settings live under `[pidos]` as `monitor=lcd`, `lcd_mode=1024x768`,
`lcd_aspect=4:3` (or `original`).
PID 1 strips that section from the guest configuration and writes the resolved
profile to `/run/pidos-video.conf`, loaded after user settings. Internal
`PI_DOS_LCD_MODE` and `PI_DOS_LCD_ASPECT` variables enable the SDL2 patch.
Explicit CRT restores the baseline below and ignores retained LCD preferences.
Existing files without `monitor` preserve their advanced settings unchanged.
Save and reboot to apply; boot can initially show the kernel's 720x400 mode
before LCD switches once to its fixed output. The new LCD rendering path still
needs physical display testing, including aspect fitting and mode changes.

### Listing available video modes

From the DOS prompt, run:

```dos
VIDEOMOD /P
VIDEOMOD /HOST
VIDEOMOD /DOS /P
VIDEOMOD > C:\TEMP\MODES.TXT
```

`VIDEOMOD` lists both groups by default. `/HOST` lists the active SDL display's
resolutions and integer refresh rates in Hz, marking the current output mode.
Entries differing only in pixel format are combined. These are the modes exposed
to the runtime by SDL/KMS, including the supplied EDID override; they do not
prove that the attached monitor accepts every timing.

`/DOS` lists the emulated adapter's BIOS modes and its advertised, supported
VESA modes with hexadecimal mode numbers, sizes and color depths. Text sizes
are columns by rows, with pixel dimensions shown separately. The VESA list
respects the active adapter, video memory and configured mode restrictions.
Applications can also program custom timings directly, which cannot be fully
enumerated here. Hercules graphics is identified separately because it is
selected through registers rather than a BIOS mode number.

`/P` pauses each screen; press Esc or Q to stop. Omit `/P` for file redirection.
`VIDEOMOD /?` displays help. The command is built into `Z:\SYSTEM\VIDEOMOD.COM`
and is available on the normal PATH without enabling diagnostics. Listing modes
does not change the display or configuration. It requires a runtime/image built
with patch `0020`; existing images must be rebuilt to include the command.

### CRT baseline

In CRT mode the DOS prompt starts in VGA 80x25 text mode with 9x16 character cells, giving
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
installation, copy the rebuilt `output/buildroot-arm64/images/Image` and
`br2-external/board/raspberrypi/cmdline.txt` onto BOOT. Physical 720x400 at 70 Hz output
still needs confirmation on the Pi and connected display.

PID 1 sets `SDL_RENDER_VSYNC=1` so the SDL2 GLES renderer requests a swap
interval of 1 for surface presentation in both CRT and LCD modes. Disabling
presentation VSync caused visible tearing and did not resolve the MIDI issue,
so it remains enabled. DOSBox-X's separate emulated VGA `vsyncmode` retains its
default `off`. Matching the HDMI refresh to the guest refresh and synchronizing
page flips are separate steps; a refresh mismatch can still cause periodic
repeated or skipped frames.

MIDI stuttering, pauses or uneven rhythm have been reported in CRT mode with
both internal synthesis and external MIDI. So far it appears limited to Sierra
games, including Leisure Suit Larry 6; AdLib playback sounds fine. The issue
occurs with the **Automatic** CPU profile; MIDI playback works correctly with
all tested fixed CPU profiles. Select a fixed profile in PIDOSCFG as a
workaround. Automatic uses `cputype=auto`, `core=dynamic_rec` and `cycles=auto`.
This narrows the investigation to automatic CPU selection/cycle handling;
the exact cause remains unconfirmed. Disabling presentation VSync did not help
and caused tearing, so keep VSync enabled.

File diagnostics are disabled by default. To enable video, boot, runtime and
network logs together, edit `C:\SYSTEM\PIDOS.CFG` from DOS (`EDIT`) and set:

```ini
[pidos]
diagnostics=true
```

Reboot to apply. No BOOT partition edit or rebuild is needed to change this
setting on an image with unified diagnostics support. Video mode tracing is
saved in `C:\SYSTEM\VIDEO.LOG` on writable PI-DOS and replaced at each
emulator start. An explicit `[log] logfile` setting overrides that path.
Set `diagnostics=false` and reboot to stop capture. Existing logs remain
untouched; verbose video mode tracing and presentation queries are skipped.
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

The shared kernel is Raspberry Pi Linux 6.12.61 at the upstream board's pinned
revision `21b410140c47ffab5668399f6f143c7d7b935c8b`, using the `bcm2711` baseline
plus 4 KiB pages and the Pi-DOS fragment. The release suffix is `-pi-dos-arm64`;
static validation checks `CONFIG_ARM64_4K_PAGES=y` directly.
Display, storage, USB, input and sound drivers are built in,
with no module loader. IPv4 and the GENET, MACB, LAN78xx and SMSC95xx Ethernet
drivers are built in for wired DHCP and network time. IPv6, Wi-Fi, Bluetooth and
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

### Wired networking and time

PID 1 starts `dhcpcd` and Chrony in the background on every boot. Wired `eth*`
and `en*` interfaces obtain an IPv4 address, default route and DNS servers through
DHCP. The native `network-dns` hook writes validated DNS addresses under `/run`;
no shell or conventional networking init scripts are required. Lease state is
also kept under `/run`, so the root filesystem can remain read-only.
The image supplies `/var/run -> /run` for dhcpcd's compiled PID/socket paths.
With `diagnostics=true` under `[pidos]` in `C:\SYSTEM\PIDOS.CFG` and writable C:,
both services log to `C:\SYSTEM\NETWORK.LOG`, replaced on each diagnostic boot. It records
DHCP leases, DNS failures and Chrony's clock corrections. With the setting absent
or `diagnostics=false`, no network log is created or modified; services
continue running and send output to serial. Read the log before rebooting if
network time does not update.

Chrony uses `pool.ntp.org` to correct the Linux clock and continues retrying if
the cable or Internet connection is initially unavailable. Initial large clock
errors may be stepped during the first three clock updates. Its `rtcsync`
setting also permits Linux to update an available RTC after synchronization.
Chrony serves no NTP or monitoring UDP port. Both daemons stop during shutdown.
The DOS prompt does not wait for DHCP or NTP: the displayed time can therefore
be wrong briefly after boot until synchronization succeeds. Offline boot still
uses the kernel's RTC restoration when a working RTC is available.

The default `[dosbox] synchronize time=true` makes DOSBox-X follow the Linux
clock, including corrections received after the DOS prompt appears. Existing
`C:\SYSTEM\PIDOS.CFG` files are preserved; if one explicitly sets this option
to `false`, change it to `true` and reboot. Manually changing the DOS date/time
can suspend automatic synchronization in DOSBox-X; `DATE /S` re-enables it.

Network time supplies UTC. Users select their local timezone using
the `[pidos]` section of `C:\SYSTEM\PIDOS.CFG`, described below; NTP does not select it automatically.
DHCP/NTP operation still requires validation on the physical Pi
with a wired connection and Internet access (DNS and outbound UDP 123).

## PID 1, storage and DOS startup

`/init` checks that it is root PID 1 before doing anything. It mounts devtmpfs,
procfs, sysfs, devpts and temporary memory-backed directories. It then waits up
to ten seconds for the data device from `pi-dos.data=` in the kernel command
line (default `/dev/mmcblk0p2`). It verifies a valid FAT32 boot sector and the
PI-DOS label before mounting. It automatically expands PI-DOS when unused
card capacity is available, as described below; it never formats a device.
The current image deliberately refuses an old ext4 data partition.

Successful writable storage supplies C: from `/dosdata/c`, with the DOS volume
label `SYSTEM`; the built-in Z: drive is labelled `PI-DOS`. These labels also
apply on read-only boots. D: is not created or mounted at startup, leaving the
letter available for CD-ROM images. Fresh images contain no `d` directory.
C: is a directory on the FAT32 partition, labelled
`PI-DOS` on the host. Storage detection requires this exact label; the old
`DOSDATA` label is not accepted and is not automatically renamed. Existing FAT32
cards need their data volume relabelled to `PI-DOS` before booting the updated
system. The internal `/dosdata` mountpoint and `dosdata.vfat` build filename
remain unchanged. If a writable mount fails, init attempts a read-only mount with
VFAT read-only options, and presents the drives read-only if usable.
Missing, wrongly labelled or unmountable storage produces a DOS Z: recovery
prompt, with no temporary replacement C: drive and no infinite device wait.
That prompt does not include a filesystem-repair utility.

### USB floppy drive A:

Appliance startup reserves `A:` for a USB floppy. Connect the drive before or
after boot, insert a DOS FAT12 disk, and use `DIR A:`, `COPY A:\FILE.TXT C:\`,
or `COPY C:\FILE.TXT A:\`. Discovery runs when DOS accesses `A:`; no reboot,
manual mount, unmount or background polling service is required. Remove a disk
and insert another as on a DOS floppy drive. `DIR`, changing into a directory,
opening/creating a file and explicit close/flush operations force a media check;
they do not wait for the repeated-read probe interval. With no usable disk,
the drive reports not ready. If more than one suitable drive is connected,
the first usable one in Linux device-name order is selected and retained until
its disk is removed or changed. A new disk resets the drive's current directory.

Supported candidates are removable USB mass-storage UFI drives, plus older
USB mass-storage drives whose model explicitly identifies a floppy. Ordinary
USB sticks, partitions and already mounted devices are excluded. Media must
have 512-byte sectors and a standard floppy capacity between 320 KiB and
2.88 MiB; 720 KiB and 1.44 MiB are the usual formats, subject to the USB drive's
own capabilities. File access requires a valid FAT12 BPB. `FORMAT A:` can
initialize a missing or damaged filesystem: BIOS geometry/format requests,
DOS absolute sector I/O and track IOCTLs access the disk before a BPB exists.
Full formatting fills/verifies addressable sectors and creates FAT12 using the
disk's existing capacity. It does not change the physical recording density,
create missing physical sector tracks, or support copy-protected track formats.
Booting from USB floppy and other disk utilities still require separate validation.
Manual disk changes never require `MOUNT -U` or a Linux unmount command.

The USB floppy also supplies a validated DOS drive parameter block through
INT 21h AH=32h (and the default-drive AH=1Fh path). FreeDOS FORMAT uses this
call to recognize an existing filesystem. The upstream removable-drive
rejection previously returned `0xff` even on readable disks, producing
`Invalid BPB` and `Cannot find existing format - forcing full format`, and
turning `/Q` into full formatting. The USB-specific path now refreshes the
DPB from the current medium's FAT driver after a forced media check; blank,
invalid or missing filesystems still fail. Existing readable disks can use
`FORMAT A: /Q`. FreeDOS may also choose its safe quick mode for plain
`FORMAT A:` on an existing format; use `FORMAT A: /U` to request a full format.
Unformatted media can still legitimately produce the initial warnings.
See [FreeDOS FORMAT detection](https://github.com/FDOS/format/blob/master/main.c).
Host checks cover this correction; physical FORMAT `/Q` validation remains
pending for the updated image.

`PIFLOPPY.COM` is an internal startup command enabled by PID 1 through
`PI_DOS_USB_FLOPPY=1`. It installs a removable DOS drive backed by DOSBox-X's
existing FAT driver and Linux SCSI `SG_IO`, without mounting a Linux VFAT
filesystem or copying the floppy into RAM. The kernel explicitly includes
`SCSI`, `BLK_DEV_SD` and `USB_STORAGE`. Native development builds do not claim
`A:` automatically.

SCSI media-change/not-ready responses and Linux disk sequence changes retire
the old medium. Open DOS handles retain their original FAT driver but cannot
read, flush or write once it has been retired, including when a replacement
disk or drive appears. Starting a raw write also retires the previous FAT
driver and file handles, so formatting cannot reuse its directory/FAT cache.
File data and FAT updates are committed before each DOS write or filesystem
mutation returns; close/flush also commit remaining metadata. There is no dirty
sector cache waiting for an unmount. The physical write-protect tab is read with MODE SENSE; drives that
cannot report it are used read-only. Finish copying and wait for the drive
light to go out before ejecting a disk or unplugging the drive.

Short/random and metadata reads fetch at most one physical
track. Sequential file reads grow to **32 KiB across track boundaries**, bounded
by the kernel's `queue/max_sectors_kb`. Four read windows use at most 128 KiB,
retaining FAT/directory data alongside the file stream. Writes update every
overlapping cached window so backwards seeks cannot expose stale data.
Within each DOS write or filesystem mutation, repeated updates to the same
sector are combined, and contiguous sectors are sent in requests of up to
32 KiB across physical tracks, subject to the kernel's limit.
Staging is bounded to 128 KiB and flushed before the DOS operation returns.
The built-in shell `COPY` now uses **60 KiB DOS requests** when copying a
regular file to the physical USB floppy. It identifies the opened destination
handle's drive, covering relative paths and concatenation as well as explicit
`A:` paths. For a 580,391-byte file, that reduces nonempty DOS writes from 18
to 10, committing FAT/directory updates fewer times. The transport still splits
each request into supported USB transfers of at most 32 KiB and commits before
the DOS write returns. Character-device copies and other destination drives
retain 32 KiB requests; external `XCOPY` retains its own buffer policy.
The subsequent hardware trace confirms the larger buffer and a COPY write
reduction from 31.1 to 28.3 seconds for the test file; see the comparison below.
Reads see staged updates, and any failed commit retires the medium. Full-sector
overwrites skip old-data reads; reads do not fetch an unused next sector at the
end of a request. Clean file buffers are not rewritten on close/flush.
DOS write batches now also trim unchanged metadata from transfer edges, using
confirmed on-disk contents for comparison. Unchanged sectors between changed
neighbours stay in the transfer to avoid splitting one USB command into two.
These comparison snapshots are separate from read windows,
which can contain staged writes, and use at most 32 KiB. Successful physical
reads/writes establish snapshots without extra reads; raw access, formatting
and media invalidation discard them. If every staged write is unchanged, a
real readiness command still checks disk sequence and media state. File-data,
boot-sector and raw writes are always sent to the drive.
The FAT driver additionally skips identical encoded directory entries in the
root and subdirectories. Changed file size, cluster mapping, attributes and
timestamps retain their normal immediate commits. These changes reduce
redundant updates without deferring metadata until close or adding a background
write cache. After this metadata optimization, the user reported performance
very close to Windows across several tests. No new exact timings were supplied;
the timed copy/FORMAT trace below precedes this metadata optimization.
Readiness probes during repeated accesses are coalesced within 100 ms, while
Linux disk-sequence checks still protect cached reads and every SCSI command.
Successful transfers restart the probe interval after completion, avoiding a
redundant readiness command immediately after a slow read or write.
Formatting writes one track per USB command as well.
BIOS read/write/verify requests transfer whole tracks directly, and the USB
drive is exempt from DOSBox-X's synthetic floppy throughput delays. The actual
USB READ/WRITE reports media changes and missing disks without a separate
readiness command between sectors. Other virtual drives retain their timing.
This reduces command/rotational overhead compared with one USB command per
512-byte DOS read and an unconditional readiness probe per file operation.
If firmware rejects a larger request with ILLEGAL REQUEST, or SG_IO rejects
its size with EINVAL/EMSGSIZE, that medium automatically falls back to requests
within physical tracks and then smaller sizes if needed. No fallback retries
media changes, missing media, write protection or transport failures. BIOS
format/read/write/verify and DOS track IOCTLs retain their explicit track
requests, while also respecting a smaller kernel transfer limit.
With the earlier file-I/O improvements, a user-reported 580,391-byte copy took
about 30 seconds reading and 45 seconds writing on Pi-DOS, versus 20 and 25
seconds respectively with the same drive/file on Windows. These are approximate
wall-clock measurements, rather than a controlled benchmark.

The shared Linux USB core now caps interrupt-IN completion polling at **8 ms**
for mass-storage UFI/CBI floppy interfaces. The cap matches the USB interface
class/protocol, not a Toshiba/TEAC model ID or a Pi board. It applies before the
host controller configures its endpoint schedule, covering both the DWC and
xHCI paths across all Pi models supported by the ARM64 image. Full-speed USB
uses a maximum `bInterval` of 8; high/super-speed USB uses 7, which encodes 8 ms.
Already faster endpoints and other storage protocols retain their intervals.
Low-speed USB and endpoints too small for the two-byte status reply are excluded.
CBI command status, SCSI errors, media-change detection and DOS write commits
remain in place. See [Linux polling patch](../patches/linux/README.md).

`BOOT.LOG` records `Pi-DOS: USB floppy status polling endpoint ... (8ms cap)`
when an interval is shortened; the runtime's endpoint log reports the adjusted
descriptor. The subsequent user log confirms the cap is active and reduces
control requests to roughly 8–12 ms, but single-track data transfers still take
about 395 ms. The logged COPY write took about 40 seconds with 137 WRITE
commands; XCOPY took about 56 seconds with 214 WRITE commands, including
76 FAT updates versus COPY's 38. Completion polling was only part of the
overhead. The bounded multi-track file-I/O changes address the remaining
track-boundary command splitting. XCOPY is not inherently faster when it
causes more committed filesystem updates.

On 28 October 2026, the user confirmed improved copy speed and noticeably
better FORMAT behavior on the Pi 5 with the Toshiba-branded TEAC FD-05PUB.
The updated trace contains successful 32 KiB file transfers and no size-rejection
fallbacks. The approximate copy-phase durations were:

| Operation | Earlier one-track transport | Multi-track transport |
| --- | --- | --- |
| COPY write | 39.8 s; 137 WRITE commands | 34.3 s; 76 WRITE commands |
| Read | 28.3 s; 67 READ commands | 26.3 s; 23 READ commands |
| XCOPY write | 56.1 s; 214 WRITE commands | 44.5 s; 153 WRITE commands |

These are durations inferred from request timestamps and gaps between commands,
not controlled benchmarks or new stopwatch measurements. The captured session
also includes 160 full-track format writes and verification reads. All 672
logged SCSI commands completed without reported status, host/driver, residual
or errno errors. The user considers copying better but possibly still slower
than Windows. This confirms improvement on the tested setup, not physical
validation of every supported Pi/drive combination or independent file-integrity
verification.

In a subsequent session with the metadata optimization, the user reported
further improvement and performance very close to Windows across several
tests. The supplied Pi 5/TEAC logs contain 1,232 SCSI commands, all without
reported status, host/driver, residual or errno errors; 32 KiB transfers are
accepted without size-rejection fallbacks and the 8 ms polling cap remains
active. No `unchanged_metadata` counter records appear in this capture, so the
trace does not quantify transport-level write suppression or isolate which
change produced the gain. This is user-confirmed comparative performance on
the tested setup, without new stopwatch figures or a controlled Windows
benchmark.

The next captured session confirms the built-in COPY policy with
`chunk_bytes=61440` records. For the isolated transfers of the approximately
580,391-byte test file, the log-derived comparison is:

| Operation | Before 60 KiB COPY | With 60 KiB COPY | Earlier Windows stopwatch |
| --- | --- | --- | --- |
| Built-in COPY write | 31.120 s | 28.322 s | about 25 s |
| Read | 25.096 s | 25.116 s | about 20 s |
| XCOPY-like write | 43.155 s | 43.154 s | not supplied |

The COPY write phase uses 22 FAT and 11 root-directory WRITE commands versus
38 and 19 previously. Metadata command time falls from 8.266 to 5.126 seconds;
data WRITE time is 22.526 seconds versus 22.077 seconds previously. Thus the
roughly 9% elapsed-time reduction agrees with the intended reduction in DOS
commit cycles. These phases are inferred from command gaps and transfer shapes,
not shell command transcripts or a controlled benchmark; the mixed multi-file
workload is not directly compared. The write phase is about 13% longer than the
earlier Windows timing, while the read phase remains about 26% longer.
All 1,252 SCSI records report no status, host/driver, residual or errno errors,
there are no size-rejection fallbacks, and the boot log confirms the 8 ms cap.
About 28.037 of COPY's 28.322 seconds is inside SG_IO, leaving little measured
host-side overhead to remove. This includes kernel/device waiting and does not
isolate mechanical time from transport/firmware behavior. No additional general
code optimization is supported by this trace. Diagnostics can be disabled in
PIDOSCFG for normal use, with only a small potential timing benefit.

A reusable worker performs the blocking USB ioctl while the emulator thread
services its idle loop, including video updates. Guest interrupts are masked
during that wait to prevent reentering disk code; USB commands remain serial.
This lets FORMAT's displayed progress refresh during device waits. The FORMAT
program still determines how often it changes the percentage itself.

For intermittent pauses, enable **Diagnostic logs** in PIDOSCFG and reboot.
`C:\SYSTEM\RUNTIME.LOG` then includes `Pi-DOS USB floppy:` command records:
`op=28` is READ, `op=2a` is WRITE, and `op=00` is TEST UNIT READY.
`elapsed_ms` measures the entire request/wait path, including worker scheduling
and emulator resumption; `gap_ms` measures time between requests, including log
output. Trace version 2 splits that path into:

| Field | Meaning |
| --- | --- |
| `queue_us` | Time before the worker enters the ioctl |
| `ioctl_us` | Wall-clock time inside the worker's `SG_IO` ioctl |
| `resume_us` | Time after the ioctl finishes until the emulator returns from its wait |
| `sg_ms` | Kernel-reported SG_IO duration, with kernel timer resolution |
| `check_us` | Time in the preceding Linux disk-sequence ioctl; outside `elapsed_ms` |
| `id`, `t_ms` | Per-medium command number and monotonic start timestamp in milliseconds |
| `region` | Region containing the starting LBA: `boot`, `fat`, `root`, `data`, `raw` before FAT validation, or `control` |

Status, host/driver result, residual length and sense bytes remain in each
record. `queue_us + ioctl_us + resume_us` approximately equals `elapsed_ms`
after conversion to the same units. `ioctl_us` includes kernel/device waiting
and scheduling, rather than exclusively time on the USB wire.

At discovery, `Pi-DOS USB floppy device:` records identify the USB vendor,
product, revision, speed, interface subclass/protocol, bound driver and power
settings. `Pi-DOS USB floppy endpoint:` records list endpoint type, direction,
packet size, hexadecimal `bInterval_hex` and the kernel-decoded
`descriptor_interval`. These are descriptor values; they do not directly
measure the host controller's actual polling schedule, and driver quirks can
override the interface's advertised protocol. Missing sysfs values are logged
as `unknown`. This metadata is read from sysfs without issuing extra USB commands.
All added runtime tracing remains disabled with diagnostics off and changes no
USB timeouts or media safety checks. The kernel's class-based 8 ms polling cap
operates with diagnostics enabled or disabled.
When a batch omits unchanged metadata, a separate `Pi-DOS USB floppy commit:`
record reports `unchanged_metadata=N`, the number of sector writes avoided in
that batch. It does not issue USB commands or replace SCSI error records.
The built-in COPY policy emits `Pi-DOS USB floppy COPY: chunk_bytes=61440`
once per source file when the larger buffer is selected with diagnostics on.
`Pi-DOS USB floppy DPB: drive=N valid=0/1` records the DPB query result when
diagnostics are enabled; `drive=0` is A:. The DPB contains filesystem fields
from the validated FAT driver rather than guessed defaults.

To investigate the measured Windows/Pi-DOS copy difference, rebuild with
`make image`, update BOOT's `Image`, enable diagnostics and reboot. Time one
write of the same test file from C: to A:, then eject/reinsert the disk and time
`COPY /B A:\BENCH.BIN NUL` using the actual filename. Retain the exact file
size and both wall-clock times, and collect `C:\SYSTEM\RUNTIME.LOG` and
`C:\SYSTEM\BOOT.LOG` from that boot before rebooting again. Large `ioctl_us`
with small queue/resume times points to the kernel/device path; large
`resume_us` points to the emulator's wait loop. A 128 ms pattern together with
an interrupt endpoint's long descriptor interval indicated the polling path
addressed by the shared kernel cap. The user log confirmed the adjusted 8 ms
interval and lower control latency, but data-transfer time remained high.
The multi-track transport addresses the separate request-size limit; the
subsequent Pi 5/TEAC hardware confirmation and trace timings are recorded above.

Host fixtures in `python3 tools/image/test-usb-floppy.py` cover classification,
FAT12/raw geometry, sequential multi-track command counts, short/random reads,
kernel queue limits, rejected-request fallback, overlapping cache consistency,
coalesced writes and commit failures, unchanged metadata comparisons (including
reverted staged changes, read overlays, raw-access invalidation and media
replacement when all writes are omitted), responsive I/O and interrupt-state restoration, diagnostic timing
decomposition and USB endpoint descriptors, BIOS format/geometry requests,
DOS sector/track IOCTL packets, protected and missing media, swaps, retired file
handles, disk-sequence changes and incomplete transfers. The adapter tests use
the patched AH=32h/1Fh dispatcher to cover existing-format detection, default
and explicit drives, blank/missing media and replacement geometry. They also
check metadata-only writes preserve file-data sectors. These checks use
simulated DOS registers/memory and a FAT-driver stand-in, not a complete DOS
`FORMAT.EXE` run. ARM64 compilation and linking check integration with the real
runtime. `python3 tools/image/test-usb-floppy-fat.py` additionally exercises
the patched upstream FAT file methods from the ARM64 build sources, covering
full/partial overwrites, sector boundaries, EOF and clean close/flush, plus
unchanged root/subdirectory entries and preservation of changed size,
allocation, attributes and timestamps. Other FAT drives retain their existing
directory-write behavior.
The FAT fixture also exercises the actual DPB serializer for DOS 3.x and 4+
layouts, including sector/cluster sizes, FAT/root/data offsets, media ID and
the unknown-free-space marker on 1.44 MiB and 720 KiB geometry.
`python3 tools/image/test-usb-floppy-copy.py` exercises the patched shell COPY
selection and loop with in-memory DOS handles, including device/other-drive
exclusions, DOS handle translation, exact/partial buffer boundaries, complete
payloads, empty files, concatenation, console Ctrl-Z and read/write errors.
The transport fixture also covers a 60 KiB DOS batch split into bounded USB
requests with a single set of metadata commits.
`python3 tools/image/test-usb-floppy-polling.py` checks the actual kernel helper's
class/protocol selection, full/high/super-speed interval encodings and retention
of faster endpoints and unrelated USB devices/transports.
Copy-speed and FORMAT improvement are user-confirmed for the Pi 5/TEAC setup.
Broader physical coverage remains pending: connection before/after boot,
write protection, swapping disks with different contents, unplug/reconnect,
independent copy/read-back integrity checks and other Pi/drive combinations.
Rebuild with `make image`; for an existing card, replace BOOT's
`Image` with the rebuilt `output/buildroot-arm64/images/Image`.

The image seeds `C:\PROGRAMS\TOOLS\EDIT.COM` with the personalized PEDIT Light binary
from `br2-external/board/raspberrypi/dos/EDIT.COM`. The default PATH selects this copy
before `Z:\DOS\EDIT.COM`. PEDIT stores its settings inside the executable, so
further preferences saved through Alt+F1 persist on writable PI-DOS. Rebuilding
the image restores the seed preferences; updating only BOOT does not replace an
existing editor on PI-DOS. Static image verification compares the packaged
editor byte-for-byte with the seed.
On an existing writable installation, startup moves `C:\SYSTEM\EDIT.COM` to
`C:\PROGRAMS\TOOLS\EDIT.COM`, preserving the executable and saved preferences.
If the destination already exists, both files are kept and PATH selects the
copy in TOOLS. Read-only boots leave existing files in place.

### USB optical drive D:

Appliance startup reserves `D:` for a USB optical drive. USB CD, DVD and Blu-ray
units qualify as hardware; this feature targets regular data CDs, audio CDs
and mixed-mode DOS game discs. Reading DVD/Blu-ray media, burning discs and
copy-protection compatibility are outside this initial scope.

Connect the drive before or after boot, insert a CD, and use `DIR D:`,
`COPY D:\FILE.TXT C:\` or a game's installer normally. D: remains registered
with MSCDEX when the drive is disconnected or empty; file access then reports
drive not ready. Discovery happens on access, with a 500 ms retry interval
when no drive is available. The first eligible device in Linux device-name
order is retained until it disconnects. Additional optical units do not get
automatic letters. Swapping discs or reconnecting does not require a manual
mount or reboot. A disc change clears the current directory, volume label,
directory iterators and sector caches. Old file handles cannot read cached
sectors or replacement-disc data after a detected change, even if both discs
have the same length.

`PICDROM.COM` is an internal startup command enabled by PID 1 through
`PI_DOS_USB_CDROM=1`. It installs a persistent DOSBox-X ISO/CD drive with a
native USB backend. Discovery requires SCSI peripheral type 5 on a USB
mass-storage interface; USB sticks, floppy drives and non-USB optical devices
are excluded. The kernel explicitly includes `BLK_DEV_SR`. Linux ISO/UDF
filesystem drivers are not required: sector reads go directly through the
read-only device using SCSI `SG_IO`, with CD track information supplied to
the existing MSCDEX implementation. Native development builds do not reserve
D: unless this environment variable and startup command are used explicitly.

CD music uses MMC digital audio extraction, rather than asking the drive to
play through an analogue audio connector. A worker serializes device I/O and
prefetches at most 32 CD audio sectors (about 0.43 seconds); the mixer never
waits for hardware. Stereo 44.1 kHz samples feed the `USBCD` mixer channel and
follow the configured Pi-DOS audio output. Games can play, stop, pause, resume,
query track/position information and control channel routing/volume through
MSCDEX. Data-track playback and ranges crossing into a data track are rejected.
If the drive rejects digital audio reads, playback stops; there is no silent
fallback to drive-controlled analogue playback. Audio extraction timing,
underruns and compatibility still need physical-drive testing.

Disc changes use SCSI unit attention, Linux's media-change indication and
device identity, rather than relying on the CD's lead-out time. Hardware
commands run on the worker with a three-second SG_IO timeout. DOS-side waits
continue servicing emulator video/timers without allowing DOS interrupt
handlers to reenter the pending operation. Audio prefetch stops and discards
its buffer after detecting removal, replacement or a read failure.

`MOUNT -U D` releases the automatic drive if D: is needed for an image or
another explicit mount. Run `Z:\SYSTEM\PICDROM.COM` with D: free to reinstall
automatic discovery. Startup never replaces an existing D: drive.

With diagnostic capture enabled, `C:\SYSTEM\RUNTIME.LOG` includes
`Pi-DOS USB CD-ROM: device=... model=...` on discovery and
`Pi-DOS USB CD-ROM: media=... tracks=... sectors=...` when a disc becomes ready.

`python3 tools/image/test-usb-cdrom.py` exercises the production Linux backend,
audio worker/mixer adapter, media refresh and DOS file-read methods using fake
sysfs/MMC responses. Coverage includes device filtering, data/raw reads and
short transfers, equal-length disc changes, stale cached file handles,
mid-read media changes, mixed-mode audio bounds, stereo sample decoding,
pause/resume, position reporting, volume/routing, eject, reconnect and an
empty/disconnected drive. The changed runtime code is cross-compiled for ARM64;
these checks do not establish physical compatibility.

Physical acceptance remains pending: connect before/after boot; list and copy
files from a real data CD and compare their contents; install and play a
mixed-mode DOS game with its CD soundtrack; check audio-CD playback,
pause/resume/volume and track transitions; swap equal-length discs; eject and
unplug/reconnect during reads and playback. Check at least a USB CD/DVD unit
and a USB Blu-ray unit. Rebuild with `make image`; an existing installation can
use the rebuilt BOOT `Image` without replacing C:.

### Editable Pi-DOS configuration

`C:\SYSTEM\PIDOS.CFG` is the single persistent configuration file for Pi-DOS
and DOSBox-X. Pi-DOS-specific options use `[pidos]`; all other sections retain
their DOSBox-X meanings. The supervisor reads the custom options and passes
the remaining sections to DOSBox-X.
Edit it with `EDIT C:\SYSTEM\PIDOS.CFG`, then reboot to apply changes. It is
loaded after the packaged defaults, before emulated hardware is initialized.
For example, change `irq=7` to `irq=5` under `[sblaster]` to select IRQ 5. The
startup table reflects effective settings, including partial override files.

Run `PIDOSCFG` for the bundled CPU profile selector, with keyboard and optional
mouse support. It sets the CPU model, execution core and speed budget while
preserving other settings, and backs up the previous file as `PIDOS.BAK`.
Run `REBOOT` after saving. See [the utility documentation](../src/doscfg/README.md)
or `C:\DOCS\PIDOSCFG.TXT` for profiles, controls and command-line options.

`Restore defaults` (F3) selects Automatic; Save is required to
persist it. The boot table shows Date above Time in the left column and the
configured DOS CPU model directly above DOS video on the right, without cycles.
Recognized profiles display their names without a prefix.
Fresh images default to Automatic (`cputype=auto`, `core=dynamic_rec`,
`cycles=auto`). Existing installations retain their configuration until
changed with setup or an editor.

Fresh images include a CRLF copy of the default configuration. Writable boots
also create the file when missing on existing PI-DOS installations. Existing
files, including empty ones, are preserved across BOOT updates. Without usable
persistent storage, the packaged defaults are used. Removing or renaming a bad
configuration from the SD card restores the defaults on the next writable boot.

`PIDOS.CFG` uses a DOS-compatible 8.3 filename so older editors save the same
file that startup loads. Startup reads only `PIDOS.CFG`; older configuration
filenames are ignored. A missing file is created from the defaults on writable
boots. Existing `PIDOS.CFG` files are preserved.

Long filename support remains enabled for applications that support it.
The default DOS compatibility version stays unchanged.
The original Z: files, folders and runtime commands remain available. No C:\CONFIG.SYS parser,
managed runtime-launcher directory or PIBOOT.BAT is added. An optional `[autoexec]`
section in PIDOS.CFG appends commands after system setup and `C:\AUTOEXEC.BAT`.

### User-selectable timezone

Edit `C:\SYSTEM\PIDOS.CFG` with `EDIT` and set an IANA timezone in the custom
section, then run `REBOOT`:

```ini
[pidos]
timezone=Europe/Madrid
```

The template uses DOS line endings and includes examples for other regions.
Section and option names are case-insensitive; `#` and `;` start comments.
The image includes
the worldwide timezone database, including daylight saving rules. Changes to
this file require no image rebuild. Zone rules can be updated through a future
image update when governments change them.

Examples: mainland Spain uses `Europe/Madrid`, the Canary Islands use
`Atlantic/Canary`, and Japan uses `Asia/Tokyo`. Fresh configurations default to
`Europe/Madrid` and DOS country `34` (Spain).
Missing, empty or invalid settings fall back to `Etc/UTC` without stopping boot.
Existing files are preserved, including during BOOT updates; writable startup
creates the template only when it is missing. Settings can also be read from
a read-only C: volume. The supervisor passes the selected timezone to DOSBox
and the network services, while the Linux clock and hardware RTC stay in UTC.

The keyboard defaults to Spanish (`keyboardlayout=sp` under `[dos]` in
`C:\SYSTEM\PIDOS.CFG`; DOSBox-X uses `sp` for Spanish). Existing configuration
files retain their saved layout.
Country and keyboard settings remain separate. For Spanish date/time formatting,
put `country=34` in the `[config]` section of `C:\SYSTEM\PIDOS.CFG`. This controls
formatting; `[pidos] timezone` controls the local clock offset and daylight saving.

### User startup commands

Fresh images include an editable `C:\AUTOEXEC.BAT` containing only `@ECHO OFF`
and comments inviting users to add startup commands. Writable boots also create
this template when the file is missing on existing installations. Existing
files, including empty ones, are preserved. Read-only and recovery boots do not
create it. The template uses DOS CRLF line endings.

Edit it with `EDIT C:\AUTOEXEC.BAT` to set environment variables, extend PATH or
start programs automatically on the next boot. Pi-DOS calls it after its standard
startup setup. The template leaves PATH, TEMP, TMP and BLASTER defaults in place.
Emulated hardware settings belong in `C:\SYSTEM\PIDOS.CFG`; changing
`SET BLASTER` alone does not reconfigure the emulated Sound Blaster. The generated
`C:\README.TXT` explains these roles.

The supplied ARJ 2.41 binary is available as `Z:\BIN\ARJ.EXE` through DOSBox-X's
standard `drivez` extension. Other upstream programs and directories retain their
original layout on Z:.

### Standard C: directory layout

Fresh images provide the following directory structure. Writable boots also
create missing directories on existing installations, preserving their contents.
`C:\README.TXT` describes the layout on fresh images.

| Directory | Purpose |
| --- | --- |
| `C:\SYSTEM` | Pi-DOS configuration, lifecycle commands and launchers |
| `C:\PROGRAMS` | Applications, with one subdirectory per application |
| `C:\PROGRAMS\TOOLS` | Self-contained utilities, including `EDIT.COM` |
| `C:\GAMES` | Games, with one subdirectory per game |
| `C:\DOCS` | User documents and manuals |
| `C:\TEMP` | Temporary files |

Writable boots set `TEMP` and `TMP` to `C:\TEMP` when that directory is usable.
Startup preserves its contents; it does not automatically delete temporary
files. User startup commands can override these variables. Read-only and
recovery boots do not set these defaults.

Doszip Commander DOS 2.68 is installed in `C:\PROGRAMS\DOSZIP`.
Type `DZ` from any directory to start it through the `C:\SYSTEM\DZ.BAT` launcher
on the default PATH. Press F1 for help and F10 to exit.

The loader (`DZ.EXE`), main program (`DZ.DOS`) and upstream help (`DZ.TXT`) stay
together. Doszip creates its writable configuration and history beside them;
the launcher sets that location explicitly. The directory also contains the
original license notice, GPL v2 text and matching source archive (`SOURCE.ZIP`).
These files are installed on PI-DOS by the complete SD image. A BOOT-only
update preserves installed applications/settings but does not populate Doszip on
an existing card; copy the program directory and launcher or use the full image.

### Automatic FAT32 PI-DOS expansion

On first boot, PID 1 grows partition 2 and its FAT32 filesystem to the remaining
SD-card capacity before mounting C:, which uses the available data space. The
image keeps an initial 512 MiB filesystem with 4 KiB clusters, MBR type `0x0c`,
and the correct hidden-sector offset. Modern Windows can open PI-DOS directly:
its `c` directory supplies C:.

Expansion requires the Pi-DOS MBR identity, 512-byte sectors, the original
BOOT partition and PI-DOS start, a valid FAT32 PI-DOS boot sector, and no
additional partitions. Other layouts are left unchanged. Cards outside the
current MBR addressing range are not automatically expanded.

The supervisor runs the non-writing `fsck.fat -n` check only when growth is
needed. It uses libfdisk to change PI-DOS's size and refresh the kernel
partition table, then uses libparted's FAT resizer while PI-DOS is unmounted.
A second FAT check verifies the result. It preserves BOOT, PI-DOS's starting
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
2 GiB and 64 GiB card images. Both kept FAT32 and the then-current DOSDATA label, preserved
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
PI-DOS `c` and `d` directories, write the new SD image, then restore those
files into the new FAT32 `c` directory, placing any former D: files in a suitable
subdirectory. Replacing only BOOT's `Image` will not
convert ext4 into FAT32. No physical card is flashed by the build tools.
Allow first boot extra time to finish expansion before the DOS prompt.

Init generates runtime startup commands under `/run`, selects a connected HDMI
DRM card (up to five seconds for discovery) and fixes audio to primary HDMI.
SDL uses `plughw:CARD=vc4hdmi,DEV=0` on single-HDMI boards, or
`plughw:CARD=vc4hdmi0,DEV=0` on dual-HDMI boards, independent of ALSA card
enumeration or EDID audio detection. The stable ID is discovered from sysfs,
with up to five seconds for driver registration. Audio does not fall back to
HDMI1 or another sound card. On dual-HDMI boards, use HDMI0 (HDMI-A-1, the
micro-HDMI connector nearest USB-C power) for the HDMI-to-VGA/audio adapter.
It requests native KMS/DRM, GLES and ALSA, attempts a
read-only remount of the volatile system root, then launches the DOS runtime as
a supervised child. A failed read-only remount is logged; the system image on
the SD card is not used as a writable Linux root filesystem.

The generated DOS environment uses the ARM64 dynrec core, US keyboard layout,
SB16 audio defaults and fixed HDMI output selected by SDL. Its default PATH is:

```dos
PATH C:\PROGRAMS\TOOLS;C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\DEBUG;Z:\PATCHING;Z:\4DOS
```

`C:\PROGRAMS\TOOLS` selects writable utilities such as EDIT, followed by
`C:\SYSTEM` for the Pi-DOS lifecycle commands and launchers. If
`C:\AUTOEXEC.BAT` exists, it runs after the system startup commands and can
override PATH.

Startup displays a BIOS-style configuration table followed by
`Welcome to Pi-DOS`. Board and Cortex processor names come from the live device
tree; CPU cores are the online core count. Host RAM is kernel-usable memory
(not the advertised installed capacity), displayed in binary units. Storage
identifies the selected PI-DOS block device, with mounted filesystem capacity,
available space and access mode. C: uses that space; only available drive
mappings are listed. DOS video, memory, audio and keyboard fields are read
from the runtime configuration and editable configuration overrides, and the Pi-DOS version comes from the init
package build version. Unavailable readings show `Unknown` or `Unavailable`.
These describe configured emulated devices, not physical HDMI/audio detection.
The table uses double-line borders shared by CP437 and CP850 so the Spanish
keyboard layout also displays the border junctions correctly.
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
`output/buildroot-arm64/images/Image`, then safely eject, boot and execute
`POWEROFF`. Also check `REBOOT`. Both SDL2 and PID 1 are embedded in the kernel;
PI-DOS does not need to be rewritten. Visual confirmation on the test board
is still required.

## Shutdown protocol

`EXIT` or `POWEROFF` exits the DOS shell. The runtime cleans up before returning
to PID 1; PID 1 syncs, unmounts PI-DOS and requests poweroff. `REBOOT` writes
the exact word `reboot` to `C:\SYSTEM\CONTROL.REQ` and exits the shell. PID 1
reads and removes that request after the runtime exits, then syncs/unmounts and
requests reboot. A stale request is removed on the next writable boot.

The reboot batch file is available on the initial writable C: image. Recovery
mode supports `EXIT`; it does not promise a DOS-issued host reboot when C: is
missing or read-only. PID 1 also handles SIGINT as reboot and SIGTERM as poweroff,
allowing ten seconds for a supervised runtime to stop before forcing termination.
Unexpected runtime crashes preserve serial diagnostics without a restart loop.

Ctrl+Alt+Delete (including keypad Delete) uses the supervised reboot path.
The SDL2 runtime intercepts the combination before it reaches the DOS keyboard,
then sends SIGINT to PID 1. PID 1 records the reboot request before asking the
runtime to stop; shutdown then syncs/unmounts storage and reboots the Pi.
This reloads `C:\SYSTEM\PIDOS.CFG`, works while a game is running, and does not
depend on C: being writable. Unsaved application data is still lost.
The shortcut is enabled only for the child launched by Pi-DOS PID 1 through
`PI_DOS_SUPERVISED=1`; standalone desktop runs keep their existing behavior.

The user confirmed successful `REBOOT`, persistence of the test file after
restart, and `POWEROFF` on the appliance. This confirms the requested functional
checks; it does not establish power-loss recovery or the deferred ALSA playback
teardown behavior.

### Pi 5 power button

A short press of the onboard power button requests the same clean poweroff as
`POWEROFF`, including while a game is running or C: is read-only/unavailable.
PID 1 listens for `KEY_POWER` on the host-bus `pwr_button` evdev device; USB
keyboard power keys are ignored. No systemd, desktop service or GPIO overlay is
needed. GPIO keyboard and evdev support are built into the kernel.

The listener ignores repeats and a button already held when it is discovered.
It rescans for late device enumeration and resynchronises after lost events
without inventing a shutdown request. The first supervised stop request keeps
its chosen action, even if a DOS `CONTROL.REQ` file is also present.
Before signalling DOSBox, PID 1 creates a volatile `/run/pi-dos-stop` marker.
The supervised runtime uses it to bypass quit confirmation dialogs and exit
through its normal teardown. PID 1 then syncs/unmounts storage and powers off.
Save application data before pressing the button.

Pressing the button while off boots the Pi through firmware. A long hold retains
the hardware forced-poweroff behavior. For lower standby consumption,
`POWER_OFF_ON_HALT=1` is an optional EEPROM setting; this implementation does
not modify EEPROM configuration. See the [official power-button documentation](https://www.raspberrypi.com/documentation/computers/raspberry-pi.html#power-button).

`python3 tools/image/test-power-button.py` checks device filtering and evdev
event handling using simulated input, without powering off the test machine.
`test-reboot-shortcut.py` also checks isolation of the shutdown marker from
standalone runs. Physical Pi 5 button shutdown/wake validation remains pending.

## Validation and first hardware test

The build runs `tools/image/verify.py` after image generation. It checks the
actual initramfs, custom AArch64 init/runtime, native SDL2 backends, ARM64 dynrec,
required built-in kernel drivers, 4 KiB pages, device trees, partition types and
PI-DOS label. It rejects an included Linux shell, BusyBox or conventional init.
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
console=serial0,115200 console=tty1 rdinit=/init loglevel=7 ignore_loglevel vt.global_cursor_default=0 pi-dos.data=/dev/mmcblk0p2
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
copy `br2-external/board/raspberrypi/cmdline.txt` to BOOT, or use this single line:

```text
console=serial0,115200 rdinit=/init loglevel=4 vt.global_cursor_default=0 pi-dos.data=/dev/mmcblk0p2 video=HDMI-A-1:720x400@70 video=HDMI-A-2:720x400@70 drm.edid_firmware=HDMI-A-1:edid/dac-crt.bin,HDMI-A-2:edid/dac-crt.bin
```

This needs no rebuild or reflash. Keep the corrected `Image` and D0 overlay.

### UART pinctrl panic on D0 boards

The original panic ends with `Asynchronous SError Interrupt` and includes
`bcm2712_pull_config_set`, `bcm2712_pinconf_set` and `brcmuart_init`. This matches
[an upstream report resolved by supplying the D0 overlay](https://github.com/agherzan/meta-raspberrypi/issues/1394).
The pinned kernel includes the overlay source; the image packaging omitted it.

For the immediate retest, copy the newly generated
`output/buildroot-arm64/images/bcm2712d0.dtbo` into the existing SD card's
`overlays/` directory on its FAT32 BOOT partition. Keep the diagnostic
`cmdline.txt` above. Safely eject the card and boot again.
No full reflash, kernel replacement or PI-DOS modification is needed.
Leave `config.txt` unchanged: the firmware selects the silicon-specific
description; do not force the D0 overlay on every Pi 5.

Future image builds include the file automatically. The user confirmed that
this change reaches the DOS prompt. Keyboard input and reconnection were also
confirmed after the PID 1 update below.

The corrected image passed static validation, including readback of the D0
overlay from its BOOT partition. Its SHA-256 is
`a3f1e209d06d497059cd71cbb661df8e7c1c1337f1f8bae3112a0e1f7f72b5fc`.

### DOS prompt appears but USB keyboard does not respond

For the Pi 4 investigation, use the diagnostic image described below to collect
kernel USB/HID messages and the runtime's open input-device handles. Working
Pi 3/Pi 5 keyboards do not establish Pi 4 VL805-controller operation.

On 25 October 2026, the Pi 4 Model B Rev 1.5 capture showed the VL805 PCIe USB
controller failing with `can't setup: -110` approximately 17 seconds after its
initial probe. No physical USB keyboard was registered. SDL KMS/DRM initialized
successfully. The captured VideoCore firmware was dated 8 December 2025,
matching the regression in
[Raspberry Pi firmware issue 2009](https://github.com/raspberrypi/firmware/issues/2009).

Replacing BOOT's matched `start4.elf` and `fixup4.dat` with the files from commit
`c3c104bcfd90a5e6f7c89f00204ac11f1e9ffa6f` (22 May 2026) restored the keyboard;
the user also confirmed substantially faster boot. This is a boot-partition
firmware fix. No board EEPROM update or SDL startup delay was needed.

The tracked profile now loads `board/raspberrypi/package-overrides.mk` from the
external tree before Buildroot expands package recipes. It pins the firmware
version and its hash file, so download names, package directories and checksum
validation all use the fixed snapshot. Run `make image-config` once after this
profile change before the next image build. Existing `.img` files must be rebuilt
to include the fix; changing the pin does not rewrite them or a physical card.
Pi 4 is confirmed with the replacement pair; Pi 3 should be retested when moving
to the newer package's `start.elf`/`fixup.dat` pair. Pi 5 uses EEPROM firmware.

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
partition with `output/buildroot-arm64/images/Image`. The initramfs and updated
PID 1 are embedded in that kernel. Keep the D0 overlay and diagnostic boot
configuration already on the card. Safely eject, boot with the keyboard
connected, and try `DIR` followed by Enter; also check reconnecting the keyboard.
PI-DOS does not need to be rewritten.

The input-correction image passed static validation. Its SHA-256 is
`41f1a41889d0123b0b7a61f62ec54cf2a8b2205eeaf9e5567183fdc4dd579f92`.

On 22 October 2026, the user confirmed that the physical keyboard works on the
appliance, including after unplugging and reconnecting. This hardware retest
confirms the correction for the reported keyboard failure. The subsequent
persistence/reboot/poweroff checks also passed, as recorded above. Mouse
validation remains deferred.

### Diagnostic image with logs on C:

Build the separate diagnostic variant with:

```sh
JOBS=4 make image-diagnostic
```

The result is
`output/buildroot-arm64/images/pi-dos-arm64-diagnostic-sdcard.img`.
It reuses the ARM64 build cache and preserves the existing normal SD image.
Its `C:\SYSTEM\PIDOS.CFG` sets `[pidos] diagnostics=true`. Its BOOT command
line requests a 1 MiB kernel log buffer and verbose console messages. The
ordinary image defaults to `diagnostics=false`; both variants use the same
configuration setting for all four logs.

After writable C: is mounted, PID 1 and the emulator create these files,
replacing the previous diagnostic boot's logs:

| DOS path | Contents |
| --- | --- |
| `C:\SYSTEM\BOOT.LOG` | Kernel-ring backlog and subsequent records, input-device names/capabilities, selected SDL/audio environment and evdev handles opened by the runtime |
| `C:\SYSTEM\RUNTIME.LOG` | SDL/DOSBox standard output and standard error, including startup failures |
| `C:\SYSTEM\NETWORK.LOG` | Wired DHCP and Chrony output, including leases, DNS failures and clock corrections |
| `C:\SYSTEM\VIDEO.LOG` | Video mode requests, available host modes and presented dimensions |

Kernel records retain their priority, sequence and microsecond timestamp prefix.
Reading `/dev/kmsg` does not clear the kernel ring. Input snapshots are checked
every five seconds and written only when their contents change. BOOT.LOG is
capped at approximately 4 MiB and flushed at least once a second when updated;
runtime output uses synchronous file writes. Normal shutdown closes the log
files before unmounting C:. No logger daemon, Linux shell or SSH service is added.
The host check is `python3 tools/image/test-diagnostics.py`.

For the Pi 4 keyboard test, boot with the keyboard attached, wait for the DOS
prompt, then unplug and reconnect it. Wait another 15 seconds. Power down, using
`POWEROFF` if input works, and open the SD card on your computer. Copy
`c/SYSTEM/BOOT.LOG`, `c/SYSTEM/RUNTIME.LOG`, `c/SYSTEM/NETWORK.LOG` and
`c/SYSTEM/VIDEO.LOG` from its PI-DOS partition before
booting it again. File flushing reduces missing output but does not make FAT32
safe against interrupted power.

An existing shared ARM64 card can retain its data: replace BOOT's `Image` with
the rebuilt `output/buildroot-arm64/images/Image`. Existing `PIDOS.CFG` files
are preserved, so add `diagnostics=true` under `[pidos]` using DOS's `EDIT`
command and reboot to enable all four logs. Set `diagnostics=false` and reboot
to disable them. No BOOT command-line logging flag is used. Existing log files
remain available but are no longer updated; they can be deleted when no longer
needed.

When first building this change with an existing Buildroot cache, run
`make -C output/buildroot-arm64 pi-dos-runtime-dirclean` before `make image`
or `make image-diagnostic` so the updated emulator patches are applied.

The logger needs a writable C: volume. Missing/read-only storage uses serial
diagnostics only. Kernel messages retained before mounting C: can be recovered;
an early kernel panic or failure to mount C: cannot be saved there. Firmware/EEPROM
messages and PID 1/storage-helper output before logging starts still require
serial capture. Runtime output is redirected to RUNTIME.LOG in diagnostic mode;
PID 1's own error messages remain on serial. This build gathers evidence; it
does not change the Pi 4 USB initialization or claim to repair the keyboard.

## References

- [Official Buildroot 2026.08 release](https://buildroot.net/download.html)
- [Buildroot manual: external customization and initramfs](https://buildroot.buildroot.org/downloads/manual/manual.html)
- The pinned release's `configs/raspberrypi5_defconfig`, `package/sdl2/`,
  `package/libudev-zero/` and `board/raspberrypi/patches/linux/linux.hash`.

Original Pi-DOS code licensing is still undecided. Complete source/license
inventory and required distribution materials before publishing an image.
