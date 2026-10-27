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
installation, copy the rebuilt `output/buildroot-arm64/images/Image` and
`br2-external/board/raspberrypi/cmdline.txt` onto BOOT. Physical 720x400 at 70 Hz output
still needs confirmation on the Pi and connected display.

PID 1 sets `SDL_RENDER_VSYNC=1` so the SDL2 GLES renderer requests a swap
interval of 1 for surface presentation. This is a scrolling experiment for
Pinball Fantasies; improvement still needs confirmation on the Pi. DOSBox-X's
separate emulated VGA `vsyncmode` retains its default `off`. Matching the HDMI
refresh to the guest refresh and synchronizing page flips are separate steps;
a refresh mismatch can still cause periodic repeated or skipped frames.
To compare without presentation VSync, change this environment value to `0`
in `src/init/init.c`, rebuild `pi-dos-init`, then rebuild the image:

```sh
make -C output/buildroot-arm64 pi-dos-init-rebuild
make image
```

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

`Restore defaults` (F3) selects Pentium 133 MHz; Save is required to
persist it. The boot table shows Date above Time in the left column and the
configured DOS CPU model directly above DOS video on the right, without cycles.
Recognized profiles display their names without a prefix.
Fresh images default to Pentium 133 MHz (`cputype=pentium`, `core=dynamic_rec`,
`cycles=fixed 71240`). Existing installations retain their configuration until
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
