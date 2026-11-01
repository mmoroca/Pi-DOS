# Pi-DOS technical reference and manual foundation

Version covered: **Pi-DOS 1.0** (`VERSION`). Review date: **30 October 2026**.
Implementation baseline: **`ff0c194`**, including all 41 development commits from
`8fb1e69`. Repository preparation and documentation changes made after that
baseline do not change the runtime described here.

This is the entry point for writing a Pi-DOS user manual. It explains the
product's purpose, architecture, current behavior, configuration, hardware
integration and limitations. It also reconciles **every feature in NOTES.md**
with the implementation and records **every development commit**. It describes
the repository as it exists, rather than treating the original wishlist or an
upstream emulator feature as proof of working Raspberry Pi hardware support.

## Contents

1. [Purpose and design approach](#1-purpose-and-design-approach)
2. [Architecture and source map](#2-architecture-and-source-map)
3. [Hardware and image layout](#3-hardware-and-image-layout)
4. [Boot, first setup and lifecycle](#4-boot-first-setup-and-lifecycle)
5. [DOS environment and bundled tools](#5-dos-environment-and-bundled-tools)
6. [Configuration and setup utility](#6-configuration-and-setup-utility)
7. [CPU, memory and PC compatibility](#7-cpu-memory-and-pc-compatibility)
8. [Video and physical displays](#8-video-and-physical-displays)
9. [Sound, MIDI and physical audio](#9-sound-midi-and-physical-audio)
10. [Storage and media](#10-storage-and-media)
11. [Keyboard, mouse, controllers and shortcuts](#11-keyboard-mouse-controllers-and-shortcuts)
12. [Networking, clock and regional settings](#12-networking-clock-and-regional-settings)
13. [Benchmarks, demos and captures](#13-benchmarks-demos-and-captures)
14. [Building, updating and provenance](#14-building-updating-and-provenance)
15. [Diagnostics, evidence and remaining validation](#15-diagnostics-evidence-and-remaining-validation)
16. [Manual-writing guide and glossary](#16-manual-writing-guide-and-glossary)
17. [Complete NOTES.md feature reconciliation](#17-complete-notesmd-feature-reconciliation)
18. [Complete development history](#18-complete-development-history)

## 1. Purpose and design approach

Pi-DOS turns a Raspberry Pi with a 64-bit Arm processor into a dedicated
DOS-compatible computer. Its goal is the **experience of using a real DOS-era
machine**: switch it on, see a DOS startup screen, reach a command prompt, run
DOS software, insert physical media, connect period-appropriate peripherals,
and shut down through the machine's own controls. A desktop, terminal window,
Linux login and general-purpose launcher are absent from the normal experience.

The implementation achieves that experience with a minimal Linux hardware
layer and a modified DOSBox-X runtime. The Pi executes Arm instructions; DOS
applications execute on an emulated x86 PC. The DOS kernel, BIOS, command shell
and virtual PC devices come from the emulator. Pi-DOS does not replace the Pi
with an electrically compatible ISA PC, run x86 instructions natively on Arm,
or provide an original Microsoft DOS installation by default. Its bare-metal
character is a product and interaction goal; Linux still schedules work and
drives the real hardware.

The project combines authenticity with conveniences that make such a computer
practical today:

- **Authentic interaction:** DOS text screens, 8.3 names by default, COM/EXE/BAT
  execution, CPU performance presets, VGA refresh-aware output, PC audio
  devices, physical floppy/CD access and external MIDI modules.
- **Dedicated appliance:** one foreground DOS runtime, custom C PID 1, direct
  display/audio/input APIs, no desktop or shell, bounded recovery paths and
  clean whole-machine reboot/poweroff.
- **Convenient modern storage:** a Windows-readable FAT32 data partition,
  automatic capacity expansion, automatic USB data volumes, ISO/image mounts
  and ZIP drives whose saves live in persistent overlay directories.
- **Accessible configuration:** a real 16-bit DOS setup program, searchable
  regional settings, backed-up text configuration and a guided first boot.
- **Selective modern services:** wired DHCP/NTP, optional guest networking,
  PNG screenshots, contemporary USB gamepads, benchmark and demoscene menus.

Compatibility takes precedence over claiming cycle accuracy. A CPU preset's MHz
label describes an approximate performance class, not a physical clock or a
promise of matching every old PC. Timing-sensitive software, unusual drivers,
copy protection, demos and Windows installations need individual validation.

Board-specific integration stays in the native supervisor and runtime adapters.
DOSBox-X changes are maintained as patches against a pinned upstream archive;
Buildroot customization lives in an external tree. This keeps the upstream
components identifiable and makes hardware policy reviewable separately from
the DOS engine.

## 2. Architecture and source map

The main execution and data paths are:

```text
Raspberry Pi firmware -> ARM64 Linux + embedded initramfs -> /init (PID 1)
                                                           |
                                    detect hardware, mount data, configure
                                                           |
                                             /pi-dos/pi-dos (DOS runtime)
                                                           |
                        emulated x86 + BIOS + DOS + shell + PC hardware
                                                           |
                                    DOS applications / PIDOSCFG / games

DOS video -> SDL2 surface/GLES presentation -> DRM/KMS -> HDMI -> display/CRT adapter
DOS sound + internal MIDI + CD audio -> emulator mixer -> SDL2/ALSA -> selected output
USB HID -> Linux evdev/libudev-zero -> SDL2 -> DOS keyboard/mouse/joystick devices
DOS files -> managed adapters -> FAT32 C:, USB FAT/exFAT, floppy, CD, images or ZIP
```

| Building block | Responsibility | Main source/configuration |
| --- | --- | --- |
| Buildroot external tree | Cross-build Linux, userspace, dependencies, image and board assets | `br2-external/`, `tools/image/build.sh` |
| Raspberry Pi firmware/DTBs | Model selection, boot and hardware description | `board/raspberrypi/config.txt`, `device-trees.txt`, firmware pin |
| Linux | Memory, scheduling, interrupts, USB, graphics, sound, block I/O and filesystems | `linux.fragment`, `patches/linux/` |
| Custom C supervisor | Mounting, configuration, discovery, startup, child reaping, device notifications and shutdown | `src/init/` |
| SDL2 and graphics/audio libraries | Native KMS/DRM, GLES, ALSA and input transport | SDL2 patches, Mesa, libdrm, GBM/EGL, ALSA |
| DOSBox-X-derived runtime | x86 emulation, DOS/BIOS/shell and virtual PC devices | `patches/dosbox-x/`, runtime package recipe |
| Native runtime adapters | Managed drives, physical media, attributes, locks, ZIPs, controllers and image switching | `src/runtime/` |
| Shared definitions | CPU/video/MIDI profiles, controller/network/display options and shortcut policy | `src/common/` |
| DOS setup program | User-facing configuration and first-boot screens | `src/doscfg/` |
| Asset importers | Download/stage checksum-pinned networking, benchmarks, demos and user-supplied MIDI assets | `tools/dos/`, `configs/*-assets.json` |
| Verification tools | Image inspection and focused host/native/hardware checks | `tools/image/`, `tools/phase0/`, `tests/` |

The supervisor runs as root and launches the runtime as its child. There is no
user-account system or privilege-separated application sandbox. The appliance
policy protects managed drive assignments, but it is not a security boundary
for executing untrusted software.

The current system uses **native SDL2**, not SDL3 or `sdl2-compat`. SDL3 was
investigated earlier and deferred until there is a complete suitable native
DOSBox-X port. The historical investigation remains in
[phase0-sdl3-history.md](phase0-sdl3-history.md).

## 3. Hardware and image layout

### 3.1 Board coverage

The shared image uses a **Cortex-A53 instruction baseline** and a **4 KiB-page
ARM64 kernel**, avoiding Pi 5-only compiler output. Model-specific firmware,
device trees and overlays accompany that common kernel.

| Family | Image target and qualifications |
| --- | --- |
| Pi 2 | Model B revision 1.2; revision 1.1 is 32-bit and excluded |
| Pi 3 | 3B, 3B+, 3A+ |
| Pi Zero | Zero 2 W; original Zero and Zero W are excluded |
| Pi 4 | 4B and 400 |
| Pi 5 | 5, 500 and 500+ |
| Compute Modules | Corresponding 64-bit CM0, CM3/3+, CM4/4S and CM5/Lite variants, with suitable carrier/USB host hardware |

Boot/input are recorded as user-confirmed on Pi 3 and Pi 5, and on Pi 4 after
the matched firmware correction. These results do not certify every board,
carrier, display, controller or peripheral in the target list. Zero/Compute
Module installations require appropriate USB host adapters or carriers.

Use a supported HDMI display or suitable HDMI-to-VGA/audio hardware and wired
USB input. The image does not configure Wi-Fi or Bluetooth. On dual-HDMI boards,
the default HDMI audio route uses **HDMI0**, nearest the USB-C power connector.

### 3.2 Partitions and persistence

The generated development SD image initially occupies about **769 MiB**:

| Location | Initial size/format | Purpose |
| --- | --- | --- |
| Partition 1, `BOOT` | 256 MiB FAT32, starts at 1 MiB | `Image`, boot firmware, DTBs/overlays, `config.txt`, `cmdline.txt` and firmware notice |
| Partition 2, `PI-DOS` | 512 MiB FAT32 | Persistent DOS files, configuration, applications, media assets and captures; expands to available card space |
| Embedded initramfs | Inside `Image` | Linux root, native programs, libraries, default config and runtime assets |
| `/run` and other temporary paths | RAM-backed | Generated configuration, device/service state and shutdown markers |

There is **no separate writable Linux root partition**. PID 1 attempts to
remount the volatile system root read-only; a failure is logged. This is more
precise than promising that the entire SD image is immutable: BOOT is an
ordinary FAT partition when edited on another computer, and PI-DOS is writable.

The host data-volume label **PI-DOS** identifies the required storage partition.
Inside DOS, the managed C: label is **SYSTEM**, and Z: is **PI-DOS**. These labels
serve different purposes. Renaming the host data partition to an arbitrary
label prevents the expected mount; `LABEL C:` cannot change that identity.

### 3.3 Automatic expansion

Before mounting C:, PID 1 checks whether partition 2 and its FAT32 filesystem
fill the card. Expansion requires the expected Pi-DOS MBR signature
`a4d05001`, 512-byte sectors, original BOOT and data starts, the expected FAT32
label and no extra partitions. Foreign layouts and cards outside the current
MBR addressing range are left unchanged.

When growth is needed, the supervisor checks FAT without repairing it, changes
the partition size through libfdisk, refreshes the kernel partition table,
resizes FAT through libparted while unmounted, and checks FAT again. Existing
files and BOOT are retained; the resizer is explicitly kept in FAT32 mode.
Repeat boots skip growth once geometry is complete. A later boot can finish
filesystem growth after an earlier partition-only expansion.

The routine never formats an unexpected device. Failed checks, resize errors
or a five-minute operation timeout lead to read-only/recovery handling.
FAT32 is not journaled, so interruption during a metadata update can still
cause damage. Host sparse-image resize checks on 2 GiB and 64 GiB images passed;
ARM64 loop-device and physical-card acceptance remain separate open checks.

## 4. Boot, first setup and lifecycle

### 4.1 Startup sequence

1. Pi firmware loads the model's boot assets and `Image`. The kernel starts the
   embedded `/init`; it does not start a Linux login or shell.
2. PID 1 verifies that it is root PID 1, mounts devtmpfs/procfs/sysfs/devpts and
   temporary paths, and prepares device notifications without a udev daemon.
3. It waits at most ten seconds for the configured data device
   (`pi-dos.data=/dev/mmcblk0p2` by default), verifies FAT32/PI-DOS identity,
   considers expansion and attempts writable then read-only mounting.
4. Writable startup seeds missing user configuration/files and directories,
   preserves existing customizations and removes stale shutdown requests.
5. Saved settings select diagnostics, timezone, display profile, physical
   audio, MIDI destination, controllers, shortcuts and guest networking. HDMI,
   USB audio and external MIDI discovery have bounded waits where required.
6. Wired DHCP and time synchronization start in the background. DOS startup
   does not wait for an Internet connection or correct network time.
7. PID 1 generates runtime configuration and DOS startup commands under `/run`
   and launches the DOS runtime with the appliance policy enabled.
8. Internal helper commands register managed C:, automatic floppy A:, optical
   D: and USB data volumes. Defaults establish PATH, TEMP/TMP and mixer levels.
9. A BIOS-style table reports physical board/CPU/RAM/storage, configured DOS
   CPU/memory/video/Voodoo/sound/MIDI, local date/time and regional settings.
10. Pending first-boot setup runs before user startup. Then `C:\AUTOEXEC.BAT`
    runs and the normal destination is `C:\>`.

The startup table describes configured emulated hardware as well as selected
host information; a Sound Blaster row is not evidence that an external DAC or
speaker is audible. Border characters are shared by CP437/CP850 to avoid
Spanish code-page junction corruption. Pi-DOS version branding also appears
in `VER`, `VER /R` and setup/welcome titles.

### 4.2 First-boot setup

Fresh images set `[pidos] first_boot=pending`. On writable C:, the welcome
screen offers keyboard layout, DOS country, timezone and CRT/LCD monitor type;
LCD adds its fixed resolution. **F2 saves and requests a clean reboot**. State
becomes `clock`, and the boot caller interprets exit code 10 by running the
supervised reboot batch file. **Esc skips**, discards pending edits, sets state
to `done` and continues with the saved defaults.

After reboot, a second screen provides a typing test, local date/time review
and optional manual DOS clock editing. F2 or Esc finishes and records `done`.
Manual clock edits affect the DOS session and can suspend host synchronization;
`DATE /S` resumes it. Setup does not wait for NTP.

Existing configurations without this key, read-only C: and recovery boots skip
the wizard. Failed saves keep the pending stage. `PIDOSCFG /FIRSTBOOT` opens
the welcome or pending clock review later; when launched manually, saving
regional settings still requires an explicit `REBOOT`.

Setup preserves an already active 80x25 text mode/page/font and draws buffered
screens through BIOS text services. The latest KMS correction re-presents the
first completed fullscreen surface after deferred EGL setup, preventing a
static welcome frame from being discarded. That specific correction still
needs physical confirmation.

### 4.3 Recovery behavior

A failed writable mount falls back to read-only C: when possible. Missing,
wrongly labelled, old ext4 or unusable data storage yields a **Z: recovery
prompt**, with no invented temporary C:. Persistent configuration, screenshots
and other writes do not silently fall back to RAM. The recovery prompt has no
general filesystem-repair tool or Linux shell. Repair/copy operations may
require another computer.

### 4.4 Clean reboot and poweroff

| Trigger | Runtime/supervisor behavior |
| --- | --- |
| `POWEROFF` or top-level shell `EXIT` | Runtime exits normally; PID 1 stops services, synchronizes/unmounts storage and requests poweroff |
| `REBOOT` | Batch file writes `reboot` to `C:\SYSTEM\CONTROL.REQ`, exits the shell, and PID 1 consumes/removes the request before rebooting |
| Ctrl+Alt+Delete, by default | Supervised runtime signals PID 1; whole-Pi clean reboot reloads saved configuration, including during a game |
| Assigned poweroff shortcut | Signals the same supervised poweroff path |
| Pi 5 onboard button, short press | Native host-bus `pwr_button` listener requests clean poweroff |
| Supervisor signals | SIGINT requests reboot; SIGTERM requests poweroff |

The supervisor marks `/run/pi-dos-stop` before stopping the runtime so guest
quit confirmation cannot block cleanup. It allows ten seconds for the runtime
to stop before forcing termination. The first accepted stop action wins;
a stale disk request cannot override it. Unexpected crashes retain serial
diagnostics without an automatic restart loop.

Ctrl+Alt+Delete includes keypad Delete, suppresses repeats and operates only
in the direct supervised child of PID 1. It is distinct from resetting only
the emulated PC. Unlike the batch-file reboot path, the supervisor shortcut
can work without writable C:. Save application data first: synchronized disk
shutdown cannot save an application's unsaved buffers.

The Pi 5 button listener ignores USB keyboard power keys, repeated presses and
a button already held during discovery; it also handles late enumeration and
lost events. A long hold retains hardware forced-poweroff behavior. Power-on
while off is firmware-controlled; Pi-DOS does not change EEPROM settings.
The recorded appliance reboot/persistence/poweroff checks passed, while the
physical Pi 5 button path still needs acceptance.

## 5. DOS environment and bundled tools

Pi-DOS boots the DOSBox-X internal DOS environment. It supports the familiar
command prompt, environment variables, file/directory commands, COM and MZ EXE
loading, BAT files, child exit codes and DOS/BIOS services. Basic COM/EXE/BAT,
relocation, variable expansion, DIR/CD/COPY and exit-code fixtures passed on
the native baseline. That evidence does not establish every command or driver.

### 5.1 Drive and directory conventions

| DOS path/drive | Intended use |
| --- | --- |
| `A:` | Automatic physical USB floppy; can be released for image use |
| `C:` | Root of the persistent PI-DOS data partition; protected managed mapping |
| `D:` | Automatic physical USB optical drive; can be released for image use |
| `E:` onward | Supported USB data volumes or explicit free-letter image/ZIP mounts |
| `Z:` | Internal runtime utilities, labelled PI-DOS; protected mapping |
| `C:\SYSTEM` | `PIDOS.CFG`, lifecycle/launcher batch files, logs and managed state |
| `C:\SYSTEM\CAPTURES` | Numbered PNG screenshots |
| `C:\SYSTEM\MIDI` | Imported SoundFonts/ROMs for internal synthesis |
| `C:\SYSTEM\NET` | Generated guest networking files |
| `C:\SYSTEM\BENCH` | Curated benchmark payload/menu |
| `C:\PROGRAMS\TOOLS` | Writable personalized editor and user tools |
| `C:\PROGRAMS\DOSZIP` | Doszip Commander, preferences, notices and source archive |
| `C:\GAMES`, `C:\DEMOS`, `C:\DOCS`, `C:\TEMP` | Applications/content, demos, documentation and temporary files |
| `C:\AUTOEXEC.BAT` | User startup customization after standard startup |

Default PATH:

```dos
PATH C:\PROGRAMS\TOOLS;C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\DEBUG;Z:\PATCHING;Z:\4DOS
```

Writable boots set both TEMP and TMP to `C:\TEMP`. Startup creates a minimal
AUTOEXEC template only if missing. Users can add variables, PATH entries or
programs there without duplicating system initialization. `C:\README.TXT`
explains the configuration/startup split.

### 5.2 Editors, file manager and utilities

`EDIT` selects the writable personalized PEDIT Light 4.00 copy at
`C:\PROGRAMS\TOOLS\EDIT.COM`, before the internal editor. Its preferences are
saved into that executable. Startup can move the former `C:\SYSTEM\EDIT.COM`
to the new location while keeping an existing destination. `DZ` launches the
two-panel Doszip Commander DOS 2.68 file manager with its settings stored beside
the program. The image retains its GPL notice and matching `SOURCE.ZIP`.
`ARJ.EXE` 2.41 is supplied at `Z:\BIN\ARJ.EXE`; its shareware provenance is
recorded in the DOS payload documentation.

`VIDEOMOD`, `ZIPMOUNT`, `IMGMOUNT`, `IMGSWAP`, `PIDOSCFG`, `BENCH`, `DEMOS`,
`NETUP`, `NETINFO`, `WFWNET`, `REBOOT` and `POWEROFF` are useful Pi-DOS entry
points described in this reference. `MOUNT` lists/ejects drives under the
managed policy. Use the built-in help and actual Z: listing for the remaining
runtime command inventory.

Fifteen redundant or inappropriate upstream utilities are deliberately
excluded: **LS, TITLE, CAPMOUSE, INTRO, SHUTDOWN.COM, BUFFERS.COM, FDISK,
CFGTOOL, DSXMENU, EDLIN, COMP, FILES, FCBS, LASTDRIV and FLAGSAVE**. Their
registrations and standalone payloads are removed; EDIT and FC remain.
Pi-DOS's own lifecycle commands replace the removed SHUTDOWN utility.

No BASIC/Pascal/C/assembly development suite is installed as a managed product
feature. Users may supply compatible DOS tools. The Open Watcom compiler used
to build PIDOSCFG is a development-host tool, not a bundled DOS programming IDE.

### 5.3 Names, DOS version and startup compatibility

The appliance shell presents **uppercase 8.3 aliases by default**, including
DIR/TREE, prompt/current directory, completion and operation feedback. Original
host names and long-filename APIs are retained; the baseline enables `lfn=true`.
`DIR /-Z` requests the long-name view; `DIR /Z` returns to short names.
Applications control their own displays. DOS paths remain case-insensitive.

The Pi-DOS product version and reported DOS compatibility version are separate.
`VER` identifies Pi-DOS; `VER /R` also retains upstream runtime/build information.
DOS version reporting remains configurable through the engine's facilities.

AUTOEXEC behavior is explicitly integrated, while CONFIG.SYS compatibility is
limited to what the internal DOS configuration and compatible driver-loading
facilities implement. Copying a PC's CONFIG.SYS does not replace the Linux boot
sequence. Booting a separately supplied DOS image changes the DOS-side startup
rules and needs its own validation.

## 6. Configuration and setup utility

### 6.1 Persistent configuration model

The authoritative user file is **`C:\SYSTEM\PIDOS.CFG`**. It combines normal
DOSBox-X INI sections with Pi-DOS-specific `[pidos]` settings. The embedded
`configs/pi-dos.conf` supplies defaults; the supervisor generates runtime
configuration under `/run`. Existing user files are preserved rather than
replaced on each boot. Older ARM-DOS names and DOSBOX.CFG references in history
are superseded by the current Pi-DOS naming and PIDOS.CFG workflow.

Use `PIDOSCFG` for supported controls or `EDIT C:\SYSTEM\PIDOS.CFG` for advanced
settings. Save, then **REBOOT** for hardware, display, input, network and regional
changes. The live mixer is the documented immediate exception.

### 6.2 Setup interface

PIDOSCFG is original C89 code compiled into a real-mode **8086-compatible
16-bit MZ executable**. It requires conventional memory, BIOS keyboard/text
services and optionally an INT 33h mouse driver, without an extender, DPMI,
x87 or third-party UI framework. It also supports Hercules monochrome text.
Its eight pages are **CPU, Memory, Video, Sound, Mixer, Input, Network, System**.

| Control | Operation |
| --- | --- |
| F5 / F6 | Previous/next page |
| Tab / Shift+Tab | Move focus |
| Enter | Edit/activate the focused control |
| Arrows, Home/End, PgUp/PgDn | Navigate lists; typing filters searchable selections |
| F2 / Alt+S | Save edited settings across all pages |
| Esc / cancel controls | Cancel without saving; restore live mixer preview when applicable |
| F3 / Alt+R | Restore defaults on the current page only, pending Save |

The System page scrolls to additional fields and shows arrow/More indicators.
Mouse selection is optional. Exit restores the previous 80x25 text screen and
cursor when applicable.

### 6.3 Current defaults

| Area | Fresh-image / corresponding Restore-defaults behavior |
| --- | --- |
| CPU | Pentium 120 MHz: `pentium`, `dynamic_rec`, `fixed 64180` |
| RAM | 16 MiB |
| Video | S3 Trio64; software Voodoo enabled, max memory enabled, Glide passthrough disabled |
| Monitor | CRT; retained LCD preferences start at 1024x768, 4:3, linear filtering off |
| Sound | Sound Blaster Pro 2, base 220h, IRQ 7, DMA 1; PC speaker enabled |
| MIDI | Roland SC-55 SoundFont profile; Intelligent MPU-401, port 330h, automatic IRQ |
| Physical audio | Primary HDMI |
| Keyboard / country / timezone | Spanish `sp` / Spain `34` / `Europe/Madrid` |
| Joystick | Two conventional two-axis joysticks; timed axes; autofire off |
| Guest network | IPX and NE2000 disabled |
| Diagnostics | Disabled |
| Reboot / poweroff / pause shortcuts | Ctrl+Alt+Delete / none / none |
| Screenshot / next CD / next floppy | Ctrl+Alt+PrintScreen / Ctrl+Alt+F4 / Ctrl+Alt+F5 |

Earlier commits used Pentium 133, Automatic and SB16 defaults. Those historical
choices must not be copied into a manual for the current baseline. Existing
installations retain saved choices until the user edits or restores them.

### 6.4 Safe saves and recovery files

Setup reloads the configuration before saving and updates only edited keys,
including duplicate occurrences. Unrelated lines and separate-line comments
remain intact. Replacement assignments use CRLF; trailing text on an edited
assignment is replaced. Custom advanced settings survive until edited.

A complete `PIDOS.TMP` is written/closed before the old config becomes
`PIDOS.BAK` and the new file takes its place. Failed replacement attempts to
restore the old file. Each successful save replaces the backup with the
immediately previous configuration. Existing temporary recovery files are
never overwritten. Read errors, read-only attributes, invalid values, files
over **24 KiB** and write/rename failures produce errors.

If a save was interrupted, inspect `PIDOS.TMP`, `PIDOS.BAK` and `PIDOS.CFG`
before deleting or renaming anything. A backup can be restored from another
computer. Cancel never writes ordinary settings.

Command-line entry points include:

```dos
PIDOSCFG /LIST
PIDOSCFG /PROFILE 486-66
PIDOSCFG /CONFIG C:\SYSTEM\TEST.CFG
PIDOSCFG /FIRSTBOOT
PIDOSCFG /HELP
```

For the full utility implementation and installed DOS manual, see
[the setup reference](../src/doscfg/README.md) and
[README.DOS](../src/doscfg/README.DOS).

## 7. CPU, memory and PC compatibility

### 7.1 CPU execution and profiles

DOSBox-X supplies x86 real/protected modes, an interpreter and an **ARM64 dynamic
recompiler**. Dynrec translates guest instruction blocks to native Arm code;
actual translated-block execution was observed with GDB on Pi 5. The CPU family
choices support software from early x86 through Pentium/MMX-era configurations,
subject to the selected model and engine limits.

| Profile ID | Display name | CPU type | Core | Cycles |
| --- | --- | --- | --- | --- |
| `auto` | Automatic | `auto` | `dynamic_rec` | `auto` |
| `286-12` | 286 12 MHz | `286` | `normal` | `fixed 1510` |
| `386-33` | 386DX 33 MHz | `386` | `dynamic_rec` | `fixed 6075` |
| `486-33` | 486DX 33 MHz | `486` | `dynamic_rec` | `fixed 12010` |
| `486-66` | 486DX2 66 MHz | `486` | `dynamic_rec` | `fixed 23880` |
| `486-100` | 486DX4 100 MHz | `486` | `dynamic_rec` | `fixed 33445` |
| `pentium-75` | Pentium 75 MHz | `pentium` | `dynamic_rec` | `fixed 40072` |
| `pentium-100` | Pentium 100 MHz | `pentium` | `dynamic_rec` | `fixed 53430` |
| `pentium-120` | Pentium 120 MHz (default) | `pentium` | `dynamic_rec` | `fixed 64180` |
| `mmx-166` | Pentium MMX 166 MHz | `pentium_mmx` | `dynamic_rec` | `fixed 95548` |

Profiles select CPU model, execution core and cycle policy together. MHz labels
are approximate equivalents; a Pi must have enough sustained performance for
the requested budget. Profiles do not independently change memory or FPU
settings. Unrecognized custom combinations remain custom, without an invented
MHz label. Automatic adapts execution speed but can change timing behavior.

A known MIDI rhythm/stuttering problem in tested Sierra software, including
Leisure Suit Larry 6, occurs with Automatic and was absent with tested fixed
profiles. A fixed profile is the current workaround. Disabling presentation
VSync caused tearing and did not resolve it. The root cause remains open.

### 7.2 Memory and software interfaces

The Memory page offers **1, 2, 4, 8, 16, 32, 64 or 128 MiB**. Selecting a value
clears `memsizekb`; saving rejects a size outside the selected CPU's address
range. More than 16 MiB can break older software, while some applications need
less. Host RAM shown in the boot table is separate from emulated DOS RAM.

Conventional memory, UMB, HMA, XMS, EMS, DPMI and DOS extender support come from
the DOS engine. Their availability depends on configuration and application
requirements. The project does not replace DOS/4GW, DOS extenders or Windows
media with bundled licensed products. TSR and device-driver compatibility is
application-dependent.

### 7.3 Virtual PC hardware and correctness work

The runtime inherits DOS INT 21h, BIOS video/disk/keyboard/timer services, PIC,
PIT, DMA, VGA registers, keyboard-controller behavior, interrupts and x86 I/O
ports. These are **emulated PC devices**, distinct from the Pi's physical USB,
GPU and sound hardware. Real peripheral integration translates between these
worlds; it does not expose arbitrary ISA hardware through the Pi.

The ARM64 FPU fix is significant beyond one game. Optimized GCC builds treated
overlapping views of an x87 register as independent, losing stack updates when
FPTAN/FSINCOS subsequently cleared C2. Wolfenstein 3D then generated corrupt
trigonometric tables with both interpreter and dynrec. Patch 0007 marks those
views as aliasing types; focused checks and the investigation are recorded in
[wolf3d-corruption.md](wolf3d-corruption.md).

Self-modifying code, timing-sensitive demos, uncommon drivers, serial software,
alternative shells and bootable external DOS images remain broader engine
capabilities requiring representative checks. Windows 3.x support is an
upstream compatibility path with specific Pi-DOS networking helpers; it is not
an automatically installed or comprehensively certified Windows system.

## 8. Video and physical displays

### 8.1 Rendering path and emulated adapters

The appliance uses SDL2 **KMS/DRM fullscreen output**, with Mesa/GBM/EGL/GLES
presentation and no X11, Wayland or compositor. The baseline sets
`output=surface`; SDL2 accelerates presentation of that surface through GLES.
This is separate from enabling DOSBox-X's own OpenGL output, shaders or Glide
passthrough; its image recipe disables the desktop OpenGL output path.

PIDOSCFG exposes IBM CGA, Hercules, Tandy, IBM EGA, IBM VGA, Tseng ET4000,
**S3 Trio64** (default), VESA VBE 3.0 (`svga_dosbox`) and VBE 2.0
(`svga_dosbox_vbe2`). The inherited VGA/text/palette machinery supports text
modes, Mode 13h, palette changes/cycling and higher-resolution SVGA/VESA modes.
Actual application timing and adapter compatibility depend on the selection.

Software **3dfx Voodoo emulation** is enabled by default with maximum memory
(the DOS guide describes 12 MiB), `voodoo_card=software`, `voodoo_maxmem=true`
and `glide=false`. The Video checkbox writes the full preset. This emulates
hardware in software; it does not enable host Glide passthrough or guarantee
useful speed/compatibility for every accelerated game.

### 8.2 CRT profile

CRT is the default and follows the guest's rendered raster/refresh. Initial
80x25 VGA text with 9x16 cells yields **720x400 at roughly 70 Hz**. The baseline
uses `fullresolution=original`, `char9=true`, `aspect=false`, `scaler=none`,
`modeswitch=true` and KMS initial-mode requests on both HDMI connectors.

The mode-selection patch uses the final rendered dimensions, including pixel
duplication, then picks an exact-size advertised host mode nearest the guest's
refresh. Without that correction, the selected size/rate could reflect an
intermediate raster or previous mode. A display/adapter still has to accept
that signal. Native-looking output on a PC CRT has user confirmation in the
video notes; that does not validate every timing, adapter or display.

Pi-DOS does not add CRT shaders, artificial scanlines, phosphor persistence or
shadow masks. The intended CRT effect comes from physical display hardware.
CRT mode intentionally avoids software aspect correction; physical monitor
geometry determines the perceived shape.

### 8.3 Custom EDID and presentation fixes

The embedded custom EDID retains the source adapter's modes/audio capability
and adds useful VGA-era progressive timings: 640x400 and 640x350 near 70.100 Hz,
720x348 near 70.082 Hz and 640x350 at 60 Hz. The Hercules-sized 720x348 signal
uses VGA-compatible scan timing, not original Hercules electrical timing.
The 60 Hz 640x350 mode retains approximately 31.5 kHz horizontal scan through
larger vertical blanking.

The generator upgrades EDID 1.2 to **1.3** and clears reserved digital-input
bits so Linux parses HDMI/audio capability rather than treating the connection
as DVI. Both HDMI connectors reference the EDID firmware inside initramfs.
Copying the binary to BOOT alone does not make it available to the kernel
firmware loader. See [the board guide](buildroot-pi5.md#custom-dac-edid).

An earlier display-dimension patch rejects invalid desktop dimensions on
headless/KMS startup, fixing draw bounds and allowing GPU validation. SDL2 and
runtime cursor patches suppress the host arrow throughout startup/teardown;
DOS applications keep their own emulated cursor. The latest first-frame patch
handles static first-boot setup after KMS surface recreation.

### 8.4 LCD profile

LCD keeps a manually selected host resolution while DOS changes modes:
**640x480, 800x600, 832x624, 1024x768 (default), 1280x720 or 1920x1080**.
It offers **4:3** aspect or **Original pixels**, which preserves the guest
raster ratio (320x200 is 8:5). The image fits with centered borders; oversized
guests shrink to fit. There is no automatic native-panel detection, integer
scale control or user refresh selector.

Nearest-neighbor scaling is default. Optional **linear filtering** enables
SDL2 bilinear sampling. LCD preferences persist while CRT is selected, and
LCD-only controls are hidden in CRT mode. Selecting a managed monitor profile
makes it authoritative over low-level SDL/render options. Old files without
`[pidos] monitor` keep their earlier advanced display behavior until selection.

### 8.5 Refresh, VSync and mode inspection

PID 1 enables `SDL_RENDER_VSYNC=1` for swap interval 1 in both profiles. The
engine's emulated VGA `vsyncmode` remains at its separate default `off`.
Presentation VSync and matching guest/host refresh solve different problems;
a refresh mismatch can still repeat/drop frames. The VSync commit addresses
periodic scrolling judder, not cycle-accurate whole-PC synchronization.

```dos
VIDEOMOD /P
VIDEOMOD /HOST
VIDEOMOD /DOS
VIDEOMOD /?
```

VIDEOMOD lists advertised host display modes and DOS BIOS/VESA modes; `/P`
pages the output. An advertised mode is not proof of physical monitor lock.
With diagnostics enabled, VIDEO.LOG records requested guest raster/rate,
rendered dimensions, host selection and presented surface/renderer sizes.

## 9. Sound, MIDI and physical audio

### 9.1 Emulated sound devices

PC speaker is independently enabled by default. Sound presets include None,
Tandy, AdLib, Creative Game Blaster/CMS, Sound Blaster 2.0, **Sound Blaster Pro 2**
(default), Sound Blaster 16 and Gravis Ultrasound Classic. Selecting a preset
enables that managed device and disables competing managed cards; MIDI remains
independent. Existing custom multi-card settings survive until a preset is
chosen.

Relevant controls include base I/O, IRQ, DMA, SB16 high DMA, OPL/FM mode, GUS
sample RAM and patch directory. Default SB Pro 2 resources are **220h, IRQ 7,
DMA 1**. Set games to the same hardware/resources; `SET BLASTER` shows the
current environment. GUS patches are not bundled; software needing them must
be supplied compatible files under the configured `C:\ULTRASND\MIDI` path.

The mixer baseline is 48 kHz, block size 1024 and prebuffer 30. Those advanced
INI controls are distinct from a dedicated latency-tuning UI, which is absent.
OPL2/OPL3, digital DMA and other device behavior derive from the emulator.
Long-duration timing and audible physical output require application/hardware
checks beyond short automated tone capture.

### 9.2 Physical output selection

System selects `[pidos] audio_output=hdmi|jack|usb`, applied after reboot:

| Selection | Behavior |
| --- | --- |
| HDMI (default) | Primary HDMI ALSA ID, `vc4hdmi` on single-HDMI or `vc4hdmi0` on dual-HDMI boards |
| 3.5 mm jack | Pi 3/4 Headphones card; requires a board/socket and enabled analogue driver/firmware node |
| External USB DAC | Playback-capable USB audio-class card; first ALSA card ID alphabetically, then lowest playback-device number |

USB discovery waits up to five seconds; connect the DAC before boot. Missing
selected output stays silent for that boot rather than switching outputs.
All internally mixed sound, synthesized MIDI and extracted CD audio follows
this selection. Physical jack/USB listening acceptance remains open.

### 9.3 MIDI profiles and assets

| Profile | Backend and requirements |
| --- | --- |
| None | Disable managed MIDI output |
| Roland SC-55 (default) | FluidSynth with `C:\SYSTEM\MIDI\SC55.SF2` |
| Roland GM | FluidSynth with `GMROLAND.SF2` |
| Roland MT-32 | Munt with matching control/PCM ROMs in `MIDI\MT32` |
| Roland CM-32L | Munt with matching control/PCM ROMs in `MIDI\CM32L` |
| External USB MIDI | ALSA sequencer destination; external module produces its own audio |

The SC-55/GM selections are SoundFont-based synthesis profiles. They must not
be described as verified hardware-accurate SC-55 chip emulation. MT-32/CM-32L
use Munt and need their respective ROM pairs. MIDI settings expose Intelligent
or UART MPU-401, default port 330h, automatic IRQ, external destination selection
and optional SysEx delay for older modules.

The user-supplied assets are identified by hashes in `configs/midi-assets.json`.
The importer places them in ignored `output/midi-assets`; ROM/SF2 payloads are
not committed to Git. Build hooks stage supplied assets into the image and data
partition, preserving existing destination files. Missing assets/devices are
reported in the boot MIDI row and disable the managed profile for that boot.
A new clone needs the required legally obtained assets staged separately.

External destinations are saved by stable identity rather than transient ALSA
client numbers. Rescan uses F5 in the picker. Interfaces lacking a unique serial
should stay on the same USB port. Ambiguous/missing destinations are not
replaced by another module. Subscriptions protect against reused client IDs.
A boot correction retries for up to five seconds **after** display/audio
detection so USB enumeration can finish.

External MIDI sends messages only: connect the module's audio to its own
speakers/mixer. Pi-DOS's internal MIDI volume cannot change that physical audio.
Select matching music hardware in each game's setup; digital SB effects and
internal MIDI can play together.

### 9.4 Live mixer

Mixer controls include master, SB digital, FM, GUS, CMS, PC speaker, Tandy,
Tandy DAC, Disney/Covox and internal MIDI. Values are left:right percentages,
**0–200**, in five-percent arrow steps or preset/custom values. Above 100 can
clip. The page queries running hardware/levels through CONFIG and MIXER.

When selected hardware matches the running setup, enabled channels preview
immediately and the page shows **LIVE**. Cancel restores starting levels at
whole-percent precision; Save retains preview and persistent settings. Pending
hardware changes suspend preview and restore original levels. Inactive channels
or unavailable live control show **AFTER REBOOT**. Queries need writable
`C:\TEMP` or `C:\`; temporary files are created exclusively and removed.
Persistent `[pidos] mixer_*` values become separate validated MIXER startup
commands before user AUTOEXEC, avoiding command-tail limits and preserving
user startup overrides.

## 10. Storage and media

### 10.1 Managed drive policy

Pi-DOS owns C: and Z:. C: maps `/dosdata` directly; Z: is the internal utility
drive. Neither can be unmounted, replaced or relocated in appliance mode,
even when C: storage is unavailable. Additional arbitrary host-directory mounts,
ordinary directory overlays and SUBST mappings are disabled. Native development
runs retain normal mounting behavior unless appliance mode is explicitly set.

Native FAT files on C: commit data, DOS timestamps and the archive bit on DOS
flush or close. File creation, deletion, directory changes and moves sync their
parent directories; moves also refresh both directory caches. Internal sharing
lookups preserve wildcard searches, so `DEL .` and `DEL *.*` continue through
all matching files. Read, write and commit failures propagate to DOS callers.
Deleting a cached regular file preserves the remaining generated `~N` aliases,
so long-named captures retain their names throughout a wildcard deletion.

`MOUNT` lists drives and can release removable mappings. `IMGMOUNT` remains
available for disk/CD images on a free letter. Automatic USB discovery skips
occupied letters, so a deliberate image or ZIP mapping is not overwritten.

```dos
MOUNT
MOUNT -U D
IMGMOUNT D C:\IMAGES\GAME.ISO -t iso
```

There is no default writable D: directory. Former D: data belongs in a suitable
C: subdirectory or other explicit mapping. An empty/disconnected automatic A:
or D: remains registered but DIR/TREE report **drive not ready**, rather than
pretending the disk is an empty filesystem. Selecting the drive letter alone
can still select the installed drive.

### 10.2 USB floppy A:

The automatic floppy adapter discovers suitable USB UFI/mass-storage floppy
hardware on access. It uses Linux SCSI `SG_IO` and the engine's FAT driver,
without Linux VFAT mounting or copying the whole disk to RAM. `PIFLOPPY.COM`
registers the backend during supervised startup. Ordinary USB sticks,
partitions and already mounted devices are excluded.

Supported media have **512-byte sectors**, standard capacity from **320 KiB to
2.88 MiB**, and valid FAT12 for file access. Common 720 KiB/1.44 MiB disks
still depend on drive capabilities. Attach before or after boot; insert, swap,
read and write through normal DOS commands. A new disk resets the current
directory. Media-change signals/device generation retire old FAT drivers and
handles so they cannot read cached old data or write a replacement disk.

```dos
DIR A:
COPY A:\LETTER.TXT C:\DOCS\
COPY C:\DOCS\LETTER.TXT A:\
FORMAT A: /Q
FORMAT A: /U
```

FORMAT is destructive. `/Q` needs a recognizable existing format; `/U` requests
full format. BIOS geometry/format requests, absolute sector access and track
IOCTLs operate even before a valid BPB exists. Full formatting fills/verifies
addressable sectors and writes FAT12 using the existing capacity; it does not
change recording density, create missing physical tracks or reproduce
copy-protected formats. A corrected DOS drive parameter block lets FORMAT
recognize readable media instead of unnecessarily forcing full formatting.

Write-protect status is read from hardware; drives unable to report it are
used read-only. File/FAT mutations commit before the DOS operation returns;
close/flush commits metadata. Read windows retain FAT/directory data and grow
sequential reads to 32 KiB, bounded by kernel transfer limits. Write batching,
redundant-sector suppression, a 60 KiB COPY destination buffer and an 8 ms
USB UFI/CBI status polling cap improve throughput while retaining checked
completion and responsive video/events.

The floppy commit records user confirmation of improved copy and full/quick
formatting on Pi 5 with a Toshiba-branded TEAC USB drive. Other documents retain
older pending statements; use that confirmation for the specific device/change,
not as blanket validation of every disk, drive or later metadata modification.
Booting directly from physical USB floppy remains a separate validation item.
Wait for successful copy completion and the drive light to go out before
removing media.

### 10.3 USB optical D:

`PICDROM.COM` installs an automatic USB optical backend registered with MSCDEX.
It accepts USB mass-storage SCSI type-5 devices, including CD/DVD/Blu-ray
**drive hardware**, while targeting **ordinary CDs**: data, digital audio and
mixed-mode game discs. DVD/Blu-ray disc reading, burning and copy-protection
support are outside the initial integration scope.

Discovery occurs on access and retries at roughly 500 ms when absent. The
first eligible Linux device is retained until disconnect; additional optical
drives do not receive automatic letters. Empty or absent hardware leaves D:
installed but not ready. Disc changes clear labels, current directory,
iterators and caches. Old handles are invalidated even for equal-length discs.
No manual mount/reboot is needed for physical disc swaps.

Data sectors use read-only SCSI I/O rather than Linux ISO/UDF mounts. Digital
CD audio is extracted through MMC, not an analogue drive connector. A serialized
worker prefetches at most 32 audio sectors, approximately 0.43 seconds. Stereo
44.1 kHz samples feed the **USBCD** mixer channel and selected physical output.
MSCDEX play/stop/pause/resume/position and routing/volume controls use that
backend. Data-track playback and ranges crossing into data tracks are rejected.
Failed digital reads stop audio instead of silently falling back to analogue.

Hardware I/O has a three-second SG_IO timeout. DOS waits service video/timers
while preventing reentry into the pending storage operation. Removal/replacement
stops audio prefetch and discards buffered samples.

`MOUNT -U D` releases D: for images; with D: free,
`Z:\SYSTEM\PICDROM.COM` reinstalls automatic discovery. Physical data-copy,
mixed-mode installation, CD audio and hotplug acceptance remain open despite
host backend/worker/MSCDEX checks and ARM64 compilation.

### 10.4 Automatic USB data volumes

USB sticks, HDDs/SSDs and card readers expose **FAT12/FAT16/FAT32/exFAT** volumes
as directory-backed DOS drives. Both unpartitioned filesystems and MBR/GPT
partitions, including MBR logical volumes, are recognized. USB ancestry and
direct-access SCSI type identify eligible devices; an external HDD does not
need a removable flag.

`PIUSB.COM` enables discovery before user AUTOEXEC. Path operations, drive
changes, free-space queries and MOUNT listings refresh it at most twice per
second; unavailable/slow volumes retry after five seconds. There is no mount
daemon. Newly found partitions are considered in partition-number order.

Each volume receives the lowest free letter from **E: upward**, skipping Z:
and other occupied letters. Connected volumes retain assignments; removal frees
a letter. Assignments are **not persisted across reboot**. Replacement gets a
fresh mount/cache/search generation; old handles fail instead of accessing the
replacement. The entire configured system disk is excluded, including a
USB-boot system disk. EFI/reserved/recovery/hidden partitions, unsupported or
conflicting filesystems, mounted devices, floppy and optical devices are skipped.

This is file-level access. DOS FORMAT/FDISK/DEFRAG cannot treat the USB volume
as a raw DOS hard disk through this adapter. Pi-DOS does not automatically
repair, repartition or format it.

Writable mounts are synchronous. Writes/truncates flush and check fsync;
close commits timestamps and filesystem state. Creation/deletion/rename and
metadata changes check flushing before success. Failures propagate to DOS/COPY.
A serialized host worker handles blocking operations while display/events
remain responsive; DOS waits for completion before reporting success.
Synchronous operation can be slower than desktop cached writes.

After a successful completed copy, no compulsory unmount command is required.
Disconnecting during a write can still lose data, and devices must honor flush
completion. Optional `MOUNT -U E` requires closed DOS files and leaves that
connection ejected until physically reconnected. Idle auto-unmounting and saved
volume-to-letter mappings are absent. Physical USB acceptance remains pending.

### 10.5 DOS attributes, timestamps and volume labels

Managed C:, USB FAT/exFAT and FAT floppies expose native **R/H/S/A** attributes.
C:/USB use Linux FAT attribute ioctls; floppies update FAT directory entries.
Directory/type/volume-label bits are preserved. Attribute reads and searches
agree with changes made on another computer. Root listings omit Linux's
synthetic parent entries, avoiding failures querying outside the DOS volume.

```dos
ATTRIB C:\DOCS\README.TXT
ATTRIB +R +H C:\DOCS\README.TXT
ATTRIB -R -H C:\DOCS\README.TXT
ATTRIB -A E:\*.TXT /S
DIR /A
DIR /AH
DIR /AS
```

Read-only files reject new writable opens, replacement/truncation and deletion
until R is cleared. An already granted writable handle retains its access.
New/modified files gain A; reading does not clear it. Hidden/system files are
omitted from ordinary searches but remain accessible through explicit paths
and appropriate attribute-aware searches. ATTRIB supports combinations,
wildcards and recursion, reports failures and returns nonzero ERRORLEVEL.
Attribute updates use checked fsync/write batches; optical/read-only media
remain protected. Timestamps follow DOS/filesystem semantics and backend
commits; they are not high-resolution POSIX timestamp promises.

Floppy LABEL updates the FAT root entry and boot-sector label; refreshed FAT
label reads also pick up FORMAT's raw-sector changes immediately. USB LABEL
flushes, ordinarily unmounts, updates with `fatlabel`/`exfatlabel`, verifies and
remounts at the existing letter. It refuses busy mounts instead of forcing a
detach and aborts on media replacement. Close open files first.

```dos
LABEL A:GAME DISK
LABEL E:USB GAMES
VOL E:
```

Labels accept at most **11 DOS bytes**, uppercased with the guest code page;
invalid characters are rejected. `LABEL E:` offers removal of an existing
label. A failed/invalid update does not intentionally erase the old label.
Unlabelled volumes report no label rather than a made-up USB label. C: SYSTEM
and Z: PI-DOS labels are protected; CD labels are read-only. Later label/attribute
integration has host regression coverage, with physical persistence checks
still distinct from the earlier confirmed floppy work.

### 10.6 Sharing and record locking

With the inherited default `[dos] share=true`, Pi-DOS enforces DOS sharing
inside its file layer across local/system, USB, FAT floppy/image and optical
handles. No separate SHARE.EXE is required. `share=false` disables this
sharing/locking enforcement, while file access permissions remain.

Opens honor compatibility, deny-all, deny-write, deny-read and deny-none modes,
checking both existing and requested grants. Independent opens can conflict
inside the same emulator. Case-insensitive short/long aliases refer to the
same object; metadata lookup does not disturb DOS search state.

INT 21h AH=5Ch implements **32-bit record offsets/lengths**, exact-range
unlocks and PSP-owned ranges. Owners can access their locked bytes; another
owner's overlapping read/write/truncate fails. Overlapping lock requests fail
even for the same owner. Zero-length requests do nothing; wrapping ranges are
rejected. The active limit is **4096 ranges**. Relevant DOS errors are **32**
(sharing violation), **33** (lock violation) and **36** (sharing-buffer overflow).

Deleting/renaming open files or directories containing them is rejected.
Creation/replacement checks grants/locks before truncation; failed opens do
not fall through to replacement. Duplicated/inherited handles keep grants
until final close. Normal process exit releases locks and FCB opens; TSR
termination retains them. Queries/unrelated closes do not release locks.

Local stream buffering and FAT metadata/sector refresh keep independent handles
coherent. A closing reader cannot overwrite newer writer data. Media generations
isolate grants from replacement disks. A later virtual-file-position fix
prevents built-in programs from failing with error 33 during loading.

These locks coordinate DOS owners inside **one emulator instance**. They are
not distributed server locks and do not establish cross-machine SMB sharing.
Host API/cache/ownership checks passed; physical/application acceptance remains.

### 10.7 Disk/CD images and disc sets

IMGMOUNT retains DOSBox-X image support, including floppy/hard-disk FAT images
and ISO or CUE/BIN CD media where supported by the pinned engine. Image format,
geometry, DOS version and booting requirements remain configuration-dependent.
Writable native disk-image behavior is distinct from a general copy-on-write
image overlay feature; only the ZIP overlay integration below has a specific
Pi-DOS persistence contract.

Load a game's full set together, after releasing the automatic letter:

```dos
MOUNT -U D
IMGMOUNT D C:\IMAGES\DISC1.ISO C:\IMAGES\DISC2.ISO -t iso
IMGSWAP D 2
```

Use CUE descriptions for mixed-mode CUE/BIN images. For floppy sets, release
A: and mount the images with `-t floppy`. The configured next-CD/next-floppy
shortcut advances and wraps **all multi-image drives of that type**, refreshing
caches and BIOS/IDE media notifications. Single images, physical USB media and
C:/Z: are excluded. Saved playlists, automatic disc choice and an in-game
selection menu are absent. Physical in-game switching still needs acceptance.

### 10.8 ZIP drives and persistent .PFS overlays

ZIPMOUNT presents a ZIP on C: or a host-backed USB volume as a DOS drive. It
requires a free letter and cannot replace managed drives or occupied letters.
The ZIP itself is always unchanged; writable changes live in an ordinary
sibling directory whose basename matches the archive and extension is `.PFS`.

```dos
ZIPMOUNT E C:\GAMES\DOOM.ZIP
E:
DOOM
C:
ZIPMOUNT E -u
ZIPMOUNT E C:\GAMES\DOOM.ZIP -ro
```

Unmodified reads use PhysFS archive access. Opening an archived file for writing
copies it into the overlay; new files/directories and saves are ordinary overlay
files. Overlay content takes precedence. Deletion markers under reserved
`PFSINFO` hide deleted archive entries across remounts, including directory
trees. Renames copy affected archived contents into the overlay. Multiple
archives have independent overlays; the same ZIP cannot mount twice at once.

`-ro` disables writes but still shows existing overlay changes and does not
create a missing overlay. A read-only source also yields read-only mounting.
A failed writable overlay creation fails the mount rather than redirecting saves
to RAM. Missing/replaced source media causes operations to fail.

Copy **both ZIP and .PFS** to retain changes. Delete the overlay only while
unmounted to reset to original content. Keep both in place while mounted and
close files before unmounting. This is a mounted DOS filesystem, not an
automatic ZIP game launcher, playlist or per-game configuration system.

## 11. Keyboard, mouse, controllers and shortcuts

USB keyboards/mice use Linux input and SDL2, with DOS scan-code/character and
INT 33h mouse interfaces provided by the emulator. Regional keyboard selection
is independent of DOS country and host timezone. Virtual uinput-to-SDL-to-DOS
checks verified BIOS scan codes, characters and modifier release. Later setup
documentation records user-confirmed keyboard/mouse hotplug; broader application
and physical mouse coverage remains less complete than basic keyboard evidence.

PID 1 forwards relevant kernel input/sound device notifications to libudev-zero
listeners. There is no udev daemon or rule system. This supplies late device
events while Linux drivers and SDL perform actual device interpretation.

### 11.1 Controllers

Input selects a stable DOS gameport type: two conventional two-axis/two-button
joysticks by default, plus four-axis, second-controller four-axis, Thrustmaster
FCS and CH Flightstick options. Appliance `auto` also means stable two-axis
hardware instead of changing with currently connected devices.

Automatic layout uses SDL2 mappings for known gamepads, then raw Linux
axes/buttons for unknown devices. Raw bypasses normalization. Choose left
stick plus D-pad, stick only or D-pad only; raw devices use their first hat.
The first two buttons can swap; advanced modes use additional mapped buttons.
A dead-zone selection applies to both controllers/sticks, while existing
per-stick values remain until changed. Timed axes and autofire expose the
engine's existing options.

Controllers can connect/reconnect after boot. An active controller keeps its
slot when another disconnects; a new one fills the first vacancy. Disconnect
releases buttons, centers axes and marks the DOS joystick unavailable. A game
that probes/calibrates only at startup may need restarting. Linux driver
support is required, and modern extra controls cannot exceed the selected DOS
joystick's capabilities. Host virtual-controller checks passed; physical game
calibration/hotplug acceptance remains open.

There is no gamepad-to-keyboard mapper, per-game profile system or gamepad-driven
launcher interface.

### 11.2 System shortcuts and screenshots

| `[pidos]` key | Default | Action |
| --- | --- | --- |
| `shortcut_reboot` | Ctrl+Alt+Delete | Whole-Pi clean reboot |
| `shortcut_poweroff` | `none` | Whole-Pi clean poweroff |
| `shortcut_pause` | `none` | Pause/resume |
| `shortcut_screenshot` | Ctrl+Alt+PrintScreen | Numbered PNG in `C:\SYSTEM\CAPTURES` |
| `shortcut_next_cd` | Ctrl+Alt+F4 | Cycle mounted CD image sets |
| `shortcut_next_floppy` | Ctrl+Alt+F5 | Cycle mounted floppy image sets |

Choices are none, Ctrl+Alt+Delete, Ctrl+Alt+F1–F10 and Ctrl+Alt+PrintScreen.
F12 remains the engine Host key. Setup rejects duplicate assignments. Boot
validation disables hand-edited duplicates in action-table order; invalid
values use defaults and can log diagnostics. Save/reboot applies changes.

Matching combinations suppress repeats; Shift/GUI variants remain available
to the guest. Print Screen also accepts Linux SysRq reporting. Pause activates
on key release so its event flushing cannot swallow the release; repeat the
shortcut or Escape to resume. Disabled shortcuts return to normal DOS input.

Screenshots capture the **DOS image before host LCD scaling/borders**. Names
use the program and a sequence such as `doom_000.png`. Numbering continues after
existing PNGs, including uppercase `.PNG` names. PNG creation does not overwrite
existing files. Shortcuts use the SDL event's
modifier snapshot and recover if another event loop consumed a key release.
PID 1 disables keyboard Magic SysRq so Linux cannot intercept and delay the
Alt+PrintScreen combination. Writable startup creates the capture directory;
unavailable/read-only C: prevents capture rather than
writing somewhere else. Host dispatch/configuration/image checks exist;
physical Pi PNG capture and in-game switching remain open. Save states, rewind
and integrated video recording are not supplied as Pi-DOS workflows.

## 12. Networking, clock and regional settings

### 12.1 Host network versus DOS network

There are two distinct layers. **Linux wired networking** obtains the Pi's own
address and supplies DNS/NTP. **Optional DOS networking** supplies emulated
IPX/NE2000 facilities to DOS/Windows applications. The latter is disabled by
default even while host DHCP/NTP runs. Offline DOS use does not require a
working cable/server, though the current image does start host services.

The kernel includes IPv4 and the relevant built-in wired Ethernet drivers.
Wi-Fi, Bluetooth and IPv6 are disabled. The image has no SSH server and is not
administered through a Linux login. Earlier SSH-driven native checks ran on a
development Linux system, not the appliance image.

### 12.2 Host DHCP, NTP and local time

PID 1 starts dhcpcd and Chrony in the background, without shell init scripts.
A native DNS hook validates received servers and writes state under `/run`.
Lease/PID/socket files are volatile; `/var/run` points to `/run`.

Chrony uses `pool.ntp.org`, retries when connectivity is absent and can step
large errors during its first three updates. `rtcsync` permits updates to an
available RTC. Chrony does not expose NTP/monitoring UDP service ports. Both
services stop during shutdown. Startup can display an incorrect time until
network sync; offline boot relies on a working RTC when available.

`[dosbox] synchronize time=true` follows the host clock, including later
corrections. Manual DOS DATE/TIME changes can suspend that behavior; `DATE /S`
resumes it. NTP supplies UTC rather than choosing a timezone.

Keyboard (`[dos] keyboardlayout=sp`), country (`[config] country=34`) and timezone
(`[pidos] timezone=Europe/Madrid`) are independent. Timezone rules supply
automatic daylight saving. Searchable catalogs come from the pinned engine's
layout/country data and installed tzdata; country can include a code page such
as `1,437`. A custom timezone must exist in the installed catalog. Local clock
and regional settings appear in the boot table.

### 12.3 IPX tunnel multiplayer

Enable IPX in Network and choose server, client or manual startup. One system
hosts, others connect to its address; the default UDP port is **213**. NETUP
retries saved startup; IPXNET commands allow manual control/PING. A firewall
must permit the selected UDP traffic. This tunnel connects compatible emulator
IPX endpoints; it does not itself place native IPX frames onto a real PC LAN.

### 12.4 NE2000 and mTCP

NE2000 defaults are I/O **300h**, IRQ **10**, random unicast MAC and SLIRP backend.
Driver settings must match. A fixed unique MAC can stabilize DHCP reservation.
The bundled packet driver and checksum-pinned **mTCP 2025-01-10** support DOS
TCP/IP. Tools include FTP, HTGET, TELNET, PING and DNSTEST, with manual, license
and corresponding source at `Z:\NETWORK\MTCP`.

SLIRP provides NAT-style guest connectivity. Default guest addressing is
10.0.2.15/255.255.255.0, gateway 10.0.2.2, DNS 10.0.2.3; these are separate from
the Pi's LAN address. Startup can use mTCP DHCP or configured static values.
TCP/UDP host:guest port forwards support inbound NAT services. SLIRP does not
carry arbitrary raw Ethernet IPX or NetBEUI broadcasts.

`MTCPCFG` points to generated `C:\SYSTEM\NET\MTCP.CFG`; DHCP updates its lease.
Generated networking files refresh each boot. Use Manual startup and a separate
user MTCPCFG for persistent custom application options. Retry is bounded so a
missing cable/lease does not indefinitely block DOS startup.

### 12.5 Direct LAN and Windows for Workgroups

PCAP sends/receives guest Ethernet frames on a selected **wired** host interface,
usually `eth0`. The guest has its own MAC/IP configuration and the switch must
permit that extra MAC. This path does not support Wi-Fi bridging.

Native IPX with real retro PCs uses PCAP, IPX tunnel disabled, and compatible
user-supplied ODI components (`LSL.COM`, ODI `NE2000.COM`, `IPXODI.COM`). The ODI
NE2000 driver is different from the packet driver. Match I/O, IRQ and frame
type; generated NET.CFG/startup files live under SYSTEM\NET. Some games also
need a supplied compatible NETBIOS driver.

Windows for Workgroups 3.11 can use PCAP with its own NE2000 NDIS/protocol
stack. Supply/install Windows and any required Microsoft TCP/IP-32 package,
configure the same adapter resources, select Windows startup in PIDOSCFG and
run **WFWNET before WIN**. WFWNET loads the supplied installation's IFSHLP.SYS
through DEVICE.COM and runs NET START; do not first load DOS packet/ODI stacks.
An externally booted DOS installation uses its own CONFIG.SYS/AUTOEXEC instead.

Real LAN access does not add SMB2/3 to WfW. File sharing needs peer protocols
and authentication compatible with that historical client. Windows media,
TCP/IP-32 and native ODI/IPX drivers are not bundled. Physical Pi LAN/WfW
sharing acceptance remains open; successful compilation or host connectivity
checks do not certify a complete Windows installation.

Use `NETINFO` for settings/status and `NETUP` to retry DOS startup. Full examples
and the installed manual are in [networking.txt](networking.txt), also available
as `C:\DOCS\NETWORK.TXT` and `Z:\NETWORK\NETWORK.TXT`.

## 13. Benchmarks, demos and captures

### 13.1 Benchmark pack

`BENCH` opens a text menu under `C:\SYSTEM\BENCH`; Q returns to the caller's
drive/directory. It deliberately preserves the current CPU/RAM/video settings.

| Menu | Benchmark |
| --- | --- |
| 1 | Superscape 3DBench 1.0c |
| 2 / 3 | Chris's 3D Benchmark: 320x200 VGA / 640x480 VESA |
| 4 / 5 | PC Player: 320x200 VGA / 640x480 VESA |
| 6 / 7 | Doom 1.9 shareware DEMO3, minimum/maximum detail, DOOMFPS result conversion |
| 8 / 9 / A | Quake 1.06 shareware DEMO1, 320x200 / 360x480 / 640x480 |

Doom/Quake benchmark sound is disabled. Quake needs an FPU, higher modes need
a suitable VESA adapter, unsupported modes can fall back, and its demo/switch
arguments retain lowercase spelling. The launcher selects video mode before
running timedemo. At the end read the console FPS, then QUIT to return.

Record Pi model, Pi-DOS version, CPU/cycles, RAM, video adapter, monitor settings,
option and result. Scores measure the configured emulated environment; cooling,
throttling and host load matter when requested speed cannot be sustained.
The MHz label does not guarantee a matching original-PC score.

The pack uses pinned Phil's DOS Benchmark Pack v1.6 inputs plus complete original
shareware installers/docs. Executables/game data are verified against the
compilation. `PHIL.TXT`, `SOURCE.TXT`, `FILES.JSN`, original installers and
DOOMFPS sources retain provenance. The selection omits old 3DBench 1.0,
cache/VGA tuning tools, CPU diagnostics and Display Doctor/UniVBE. Packaging
checks do not replace physical benchmark acceptance or distribution review.

### 13.2 Demoscene pack

`DEMOS` opens a numbered text menu under `C:\DEMOS`. H shows setup notes and Q
returns. It includes **Second Reality, Unreal, Panic, Crystal Dream I,
Crystal Dream II, Stars: Wonders of the World, Caero, Inside and Fishtro**.
Inside opens its original SETUP first. The menu changes no emulator settings.

Use SB Pro/SB resources matching Pi-DOS; select SB16 in a production only after
configuring SB16. Tracker music uses the selected sound card rather than the
independent SC-55 MIDI profile. Some productions need EMS or extended memory;
start with 16 MiB and the default profile, consult original notes and try an
appropriate fixed 486 profile when timing is problematic.

Original scene.org archives are checksum-pinned and retained under each
production's `ORIG` directory, including both Second Reality disks. Stars uses
the corrected 2014 DOS executable and Caero its final release. SOURCE.TXT and
FILES.JSN record provenance/inventory. Archive integrity and launch files were
checked; physical playback, sound synchronization and speed remain unverified.

`make demos` creates `output/pi-dos-demos.zip`. To add it to an existing card,
merge its DEMOS directory and SYSTEM\DEMOS.BAT into the PI-DOS volume root,
keeping other SYSTEM files. Future image builds include the pack automatically.
This is a simple DOS menu, not a general-purpose game library browser.

## 14. Building, updating and provenance

### 14.1 Pinned inputs

| Component | Repository-selected input |
| --- | --- |
| Buildroot | 2026.08, archive SHA-256 in `sources.lock.json` |
| DOSBox-X | `4f19017c5f565dc40d01fede0f1382892e7243d7`, v2026.08.31 baseline |
| Native SDL2 | 2.32.10, revision `5d249570393f7a37e037abf22cd6012a4cc56a71` |
| Raspberry Pi Linux | 6.12.61 baseline, revision `21b410140c47ffab5668399f6f143c7d7b935c8b` |
| Pi boot firmware | `c3c104bcfd90a5e6f7c89f00204ac11f1e9ffa6f`, 22 May 2026 snapshot |
| DOS setup compiler | Open Watcom `2026-10-01-Build`, toolchain archive/hash pinned separately |
| MIDI/network/demo/benchmark assets | Per-file/archive manifests under `configs/` |

These are facts about the checked-in build inputs, not claims about the latest
upstream releases. Buildroot package hashes cover its selected dependencies.
The build enables reproducible-build controls and verified inputs; this does
not establish independently demonstrated byte-identical full-image rebuilds.

### 14.2 Main build workflows

The development host needs Python 3.12+, C/C++ tools, Make/Patch, Autoconf,
Automake, pkg-config, CMake/Ninja and appropriate development dependencies.
Native graphics/audio work needs ALSA, libdrm, GBM, EGL/GLES headers/libraries.
Image packaging needs Buildroot host prerequisites and `bsdtar` to extract the
original Quake installer without executing it.

```sh
make image-config
JOBS=4 make image
# Alternative diagnostic variant, with persistent logging enabled by default:
JOBS=4 make image-diagnostic
```

Outputs are under `output/buildroot-arm64/images/`:
`pi-dos-arm64-sdcard.img` or `pi-dos-arm64-diagnostic-sdcard.img`, plus `Image`
and other build artifacts. Source downloads/cache live in ignored `dl/` and
`output/`; neither belongs in a Git source sync. The tools generate images;
they do not flash a physical card or update EEPROM.

A fresh clone must stage required user-supplied MIDI assets through
`tools/dos/midi-assets.py --source /path/to/authorized/assets`. The exact files
must match the manifest. Downloads for other packs are checksum-verified and
cached. Offline operation of the **runtime** is distinct from an **offline
build**, which requires every necessary archive/asset already cached.

Native baseline development uses:

```sh
make fetch
make sdl2
make phase0
bash tools/phase0/build.sh --offline
```

This creates a host-native runtime and private SDL2, not the complete appliance
image. The early phase-zero build has a narrower internal-MIDI feature set.
Do not reuse earlier Cortex-A76 `output/buildroot-pi5` binaries for the shared
Cortex-A53 image.

To rebuild setup, bootstrap the pinned local Watcom toolchain then run
`make dos-config`. The checked-in executable allows ordinary image builds
without installing a DOS compiler. `PIDOSCFG.build.json` records source,
compiler-driver and executable hashes; image verification rejects stale code
or a mismatched packaged executable. See [setup build details](../src/doscfg/README.md#rebuilding).

### 14.3 Image verification and patch maintenance

The runtime recipe applies the ordered DOSBox-X patch series, copies native
integration headers into the pinned source, generates version branding and
checks that required MIDI/network/screenshot features compiled. Separate SDL2,
Linux and Parted patches handle cursor suppression, USB floppy polling and FAT
resize correctness. Incremental fingerprints force runtime rebuilds when patch
or adapter inputs change.

The image verifier checks partition geometry/labels, boot assets/overlays,
kernel settings, dependencies/backends, absence of unwanted Linux interfaces,
setup provenance, configuration, required commands and packaged asset inventories.
Benchmark verification also checks Quake demo references in PAK data; demo
verification checks launch paths and retained archives.

A successful image inspection proves packaging properties, not a physical Pi
boot, display lock, audible sound, peripheral compatibility or application
correctness. Run targeted checks when changing the relevant implementation,
and use the documented hardware procedures for physical acceptance.

### 14.4 Updating and migration

For an already compatible FAT32 volume-root installation, many runtime updates
can replace BOOT's **Image** while retaining C:. Changes to firmware, DTBs,
overlays, cmdline or seed data may need those matching assets too; consult the
particular change. Settings and user files generally survive because startup
seeds only missing files. Updating Image does not install every newly added
data pack into an existing C: automatically.

Older ext4 images require backing up data, writing the new FAT32 image and
restoring files. A kernel-only update does not convert filesystems. Old `c/`
and `d/` wrappers are not automatically migrated; move their contents to the
appropriate new volume-root locations. Old FAT32 `DOSDATA` volumes must be
relabelled PI-DOS for current storage detection. Existing advanced settings
without newer profile keys retain documented legacy behavior until selected.

### 14.5 Licenses and distribution boundary

A source sync and a public SD-image release are different milestones. The
project has **not yet selected a license for original Pi-DOS code**; the
Buildroot init package records it as unknown. Do not infer a project-wide
license from an upstream runtime component.

DOSBox-X is GPL-2.0-or-later; its local modifications, corresponding sources,
notices and build instructions must accompany an appropriate distribution.
Doszip ships its license/source archive. SDL2, firmware, libraries, networking
tools and compiler runtime retain their respective notices; mTCP includes its
GPLv3 notice and corresponding source. BOOT retains LICENCE.broadcom.

The tracked personalized editor and ARJ binary have recorded provenance but
need their applicable distribution conditions reviewed. MIDI ROMs/SoundFonts
are user-supplied and excluded from Git. Doom/Quake shareware data is not made
GPL by the game engine source license. Benchmark compilations and demo archive
availability do not establish blanket public redistribution rights.

Complete the component/firmware/payload inventory, notices, corresponding-source
bundle and individual asset review before publishing a distributable image.
See [LICENSES/README.md](../LICENSES/README.md). No new license is selected by
this documentation or repository-preparation work.

## 15. Diagnostics, evidence and remaining validation

### 15.1 Unified optional logging

Normal images default to diagnostics off. In PIDOSCFG enable Diagnostic logs,
or edit this setting and reboot:

```ini
[pidos]
diagnostics=true
```

| Persistent log | Contents |
| --- | --- |
| `C:\SYSTEM\BOOT.LOG` | Kernel/supervisor boot and storage/lifecycle diagnostics |
| `C:\SYSTEM\RUNTIME.LOG` | Runtime output and hardware/backend errors |
| `C:\SYSTEM\NETWORK.LOG` | Host DHCP/DNS/time-service output |
| `C:\SYSTEM\VIDEO.LOG` | Detailed mode requests/selection/presentation |

Enabled logs are replaced on the next relevant boot/runtime startup; copy
useful evidence before rebooting again. Disabled logging leaves old files
untouched and skips verbose video queries; it does not disable mode selection
or networking. An explicit engine `[log] logfile` can override the video path.
Missing/read-only storage prevents persistent logs without preventing runtime
startup; serial diagnostics remain available.

The diagnostic image enables the same controls by default. Historical
`armdos.diagnostics` boot flags and separate `videodiagnostics` switches have
been superseded by PIDOS.CFG. Default kernel diagnostics go to serial; HDMI
console diagnostics can be enabled temporarily through the board procedure.

### 15.2 Evidence levels and known gaps

| Area | Evidence recorded before this documentation | Practical limit |
| --- | --- | --- |
| Native DOS execution | Host/Pi 5 COM/EXE/BAT, file operations and interpreter/dynrec checks; translated ARM64 block observed | Small fixtures are not an application compatibility suite |
| Shared image boot/input | User confirmation on Pi 3/Pi 5; Pi 4 after matched May 2026 firmware | Remaining models/carriers need hardware checks |
| Lifecycle/persistence | User-confirmed appliance REBOOT, persisted test file and POWEROFF | Does not prove power-loss recovery or physical button behavior |
| Graphics | Pi 5 GPU readback, aspect/color and KMS flips; user CRT appearance report | Latest profiles/static welcome and every mode/display remain separate |
| Input | Virtual BIOS/DOS key path; user keyboard/mouse hotplug record | Physical gamepad calibration/hotplug remains pending |
| Audio | Short PC speaker/OPL2/SB16 tone/capture/playback-status checks | One teardown XRUN; long audible/jack/USB acceptance remains open |
| MIDI | Profiles/stable identity/retry implementation; reported fixed-profile workaround | Automatic-profile Sierra timing issue unresolved |
| Floppy | Host backend/FAT checks and specific Pi 5 TEAC drive confirmation in commit | Later labels/attributes/locking and other drives still need acceptance |
| Optical/USB storage | Host simulated hardware/sysfs/media, I/O-failure checks and ARM64 integration | Physical devices/media/audio and durable removal tests remain open |
| Attributes/sharing/ZIP | Focused real DOS API/backend checks with host simulated kernel/media as applicable | Do not certify every application or physical device |
| Networking | Configuration/backend/host connectivity and packaging checks | Physical Pi LAN, native IPX peers and WfW file sharing pending |
| FAT32 expansion | Native sparse 2/64 GiB image checks | ARM64 partition refresh and physical cards pending |
| Benchmark/demos | Hashes, inventories, selected launch data/arguments checked | Physical scores/playback/music synchronization pending |

Historical reports describe their own build, not necessarily the latest image.
When they conflict, prefer current code for current behavior, a newer explicit
record for the exact observed check, and a clear pending label for anything
not established. In particular, the floppy confirmation in `006f3d9` is more
specific than older generic pending summaries. This documentation pass does
not rerun builds or claim new hardware results.

### 15.3 Important fixes to retain in troubleshooting material

- **Pi 4 USB timeout/no input:** matched `start4.elf`/`fixup4.dat` firmware pin
  corrected the approximately 17-second controller timeout; replace a matched
  pair rather than mixing firmware versions.
- **Pi 5 D0 early UART pinctrl panic:** BOOT must include `bcm2712d0.dtbo` as
  well as the relevant DTB; some EEPROM paths need the overlay.
- **KMS bad drawing bounds:** reject invalid desktop dimensions before layout.
- **Black first-boot welcome:** latest patch re-presents a static first frame;
  BIOS-buffered setup redraws preserve text mode and improve visibility.
- **HDMI audio treated as DVI:** EDID 1.3 capability parsing and primary HDMI0
  routing are distinct parts of the correction.
- **Wolfenstein corruption:** ARM64 optimized x87 aliasing fix, not a generic
  game-data or dynrec-only workaround.
- **Automatic CPU MIDI rhythm problem:** use a tested fixed profile; leave
  presentation VSync enabled.
- **FORMAT unexpectedly full instead of quick:** corrected floppy DPB lets
  an existing FAT12 format be recognized; genuinely blank media still differs.
- **Error 33 launching built-ins:** virtual file position/locking integration
  corrected accidental loader conflicts.
- **DIR root listing stops early:** native FAT root listing skips synthetic
  parent entries outside the mapped volume.
- **Mouse arrow during teardown:** layered runtime/SDL2 suppression, with latest
  physical visual confirmation still pending.

## 16. Manual-writing guide and glossary

A future user manual should begin with what the user needs to connect, flash
and expect on first power-on, then explain the welcome, prompt, files and
safe power controls. Hardware options should use current defaults and distinguish
emulated devices from physical outputs. Keep build internals in a developer
appendix while retaining the practical consequences of recovery, missing MIDI
assets and media safety.

Suggested chapter order: introduction/hardware; installation and migration;
first-boot region/display; DOS essentials and directory layout; copying files;
CPU/RAM/video; sound/MIDI; keyboard/mouse/controllers; physical media/images/ZIPs;
networking/time; tools/benchmarks/demos; screenshots/shortcuts; shutdown/recovery;
troubleshooting; command/configuration reference; licenses and technical appendix.

Use these terms consistently:

| Term | Meaning in Pi-DOS |
| --- | --- |
| Host | Native Arm/Linux side, including real devices and hardware APIs |
| Guest / DOS environment | Emulated x86 computer, BIOS/DOS services and applications |
| PID 1 / supervisor | Native `/init` owning lifecycle, mounting and runtime supervision |
| Initramfs | Linux root embedded in Image, unpacked during boot |
| Dynrec | Guest x86 instruction blocks dynamically translated to Arm |
| KMS/DRM | Direct Linux display mode/presentation interface |
| EDID | Display/adapter mode and capability description supplied to Linux |
| ALSA | Linux audio and MIDI device interface |
| MSCDEX | DOS CD-ROM interface used by applications and the optical adapter |
| FAT BPB / DPB | On-disk BIOS parameter block / DOS drive parameter block |
| PSP | DOS process identity used for file/record-lock ownership |
| Overlay | Persistent ZIP changes in a sibling .PFS directory |
| PCAP / SLIRP | Direct guest Ethernet on wired LAN / NAT-style guest TCP/IP |
| Profile | Global emulated hardware/performance selection; not a per-game preset |

For manual claims, **implemented**, **host-checked** and **physically confirmed**
are different statements. Do not convert a wishlist mark into a compatibility
guarantee, describe SoundFont synthesis as exact SC-55 hardware, call USB data
mapping raw disk emulation, or imply that a clean shutdown saves unsaved game
state. The appendices below provide a feature-by-feature starting point and
an audit trail back to the development work.

## 17. Complete NOTES.md feature reconciliation

This appendix covers all **177** feature entries in NOTES.md at the reviewed baseline.
The original marks (`v`, `~`, `x`) are retained for traceability, not treated
as acceptance evidence. In particular, hotplug and ZIP overlays progressed
beyond older wishlist marks. “Not integrated” means no supported Pi-DOS workflow;
it does not assert that all related upstream code has been removed.

| Status | Meaning |
| --- | --- |
| Implemented | Specific Pi-DOS behavior/configuration exists; physical acceptance may remain pending as described above. |
| Inherited | Primarily an upstream DOSBox-X capability, without a separate broad Pi-DOS acceptance claim. |
| Partial | A subset, limitation, configuration dependency or significant validation boundary applies. |
| Not integrated | No supported managed Pi-DOS workflow at this baseline. |

### DOS Environment

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| MS-DOS-compatible command prompt | `v` | Implemented | Internal DOSBox-X DOS shell is the normal appliance interface; original MS-DOS is not bundled. |
| DOS commands (DIR, COPY, DEL, etc.) | `v` | Implemented | Core shell/file commands are available; small DIR/CD/COPY fixtures passed. See §5 for removed utilities. |
| COM and EXE execution | `v` | Implemented | COM and MZ EXE fixtures, including relocation/child status, passed on the baseline. |
| BAT scripting | `v` | Implemented | Batch invocation, CALL, variables and child exit-code handling are part of the checked baseline. |
| AUTOEXEC.BAT support | `~` | Implemented | User C:\AUTOEXEC.BAT runs after managed startup; seed only if missing. |
| CONFIG.SYS compatibility | `~` | Partial | Engine configuration and compatible driver loading exist; not an unrestricted original-PC CONFIG.SYS boot. |
| Environment variables | `v` | Implemented | Standard DOS environment plus managed PATH, TEMP/TMP, sound and network variables. |
| Standard folder structure for the DOS file system | `v` | Implemented | Volume-root SYSTEM, PROGRAMS, GAMES, DEMOS, DOCS and TEMP layout. |
| DOS file managers (DosZip Commander) | `v` | Implemented | Doszip DOS 2.68 and DZ launcher, writable settings and matching source archive. |
| DOS text editors | `v` | Implemented | Writable personalized PEDIT/EDIT takes PATH precedence; internal EDIT retained. |
| DOS programming tools (BASIC, Pascal, C, Assembly) | `x` | Not integrated | No managed development suite; compatible tools may be user-supplied. |
| DOS keyboard shortcuts | `v` | Implemented | Guest keyboard behavior plus supervised lifecycle/capture/media shortcuts; see §11. |
| DOS error messages and exit codes | `~` | Partial | Normal DOS errors/statuses plus not-ready, attribute, sharing and lock errors; not exhaustively compatible. |
| International keyboard layouts and code pages | `v` | Implemented | Searchable layout/country catalogs and engine code-page support; default Spanish. |
| DOS date and time management | `v` | Implemented | DATE/TIME, host synchronization, local timezone and first-boot clock review. |
| Configurable DOS version reporting | `v` | Inherited | Engine DOS compatibility version controls remain separate from branded Pi-DOS product version. |

### CPU and Memory

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| x86 emulation (8086 through Pentium) | `v` | Inherited | Engine CPU models through Pentium/MMX era; managed profiles begin at 286. |
| ARM64 dynamic recompiler | `v` | Implemented | Selected dynamic_rec build; actual translated-block execution observed on Pi 5. |
| Real-mode execution | `v` | Implemented | Required by the shell/setup; checked COM/EXE fixtures exercise it. |
| Protected-mode execution | `v` | Inherited | Engine protected-mode support; application-specific DOS extender acceptance remains necessary. |
| x87 FPU emulation | `v` | Implemented | Inherited x87 plus Pi-DOS overlapping-register alias fix for optimized ARM64 builds. |
| Conventional memory (640 KB) | `v` | Inherited | DOS memory model supplied by engine; usable free space depends on configuration. |
| Upper memory (UMB) | `v` | Inherited | Engine memory-manager capability; configuration/application dependent. |
| Extended memory (XMS) | `v` | Inherited | Engine memory-manager capability; managed RAM size does not prove every XMS client. |
| Expanded memory (EMS) | `v` | Inherited | Engine capability; some bundled demos require it for Sound Blaster music. |
| High Memory Area (HMA) | `v` | Inherited | Engine DOS memory capability; not a separate Pi-DOS hardware layer. |
| DPMI support | `v` | Inherited | Engine protected-mode services; verify particular clients/extenders. |
| DOS extenders (DOS/4GW, etc.) | `v` | Inherited | Engine compatibility path, without promising or bundling every commercial extender. |
| Configurable CPU speed and cycles | `v` | Implemented | Ten current profiles plus preserved custom engine settings; see §7 table. |
| Automatic CPU speed adjustment | `v` | Implemented | Automatic profile available; timing/MIDI issue documented, fixed profile is default. |
| Self-modifying code support | `v` | Inherited | Dynamic engine capability; extended cache/JIT and application validation remains open. |
| TSR compatibility | `~` | Partial | Engine TSR behavior; record locks retained on TSR termination; broad driver/TSR compatibility unproven. |

### Graphics and Video

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| MDA, Hercules, CGA, EGA and VGA emulation | `v` | Inherited | Engine adapter families; setup exposes a curated set including Hercules/CGA/EGA/VGA. |
| SVGA / VESA VBE modes | `v` | Implemented | S3, ET4000 and VBE 2/3 selections with BIOS mode listing. |
| DOS text modes (80×25, etc.) | `v` | Implemented | 80x25 boot/setup and inherited additional text modes; mode/font preservation fixes included. |
| Custom text fonts | `v` | Inherited | Engine font/text facilities; no new Pi-DOS font management workflow. |
| VGA Mode 13h (320×200) | `v` | Inherited | Standard emulated VGA graphics; host presentation depends on monitor profile. |
| High-resolution DOS graphics | `v` | Inherited | SVGA/VESA hardware modes subject to selected adapter, software and host output. |
| Accurate VGA palettes | `v` | Inherited | VGA DAC/palette machinery; no universal quantitative accuracy certification. |
| Hardware palette cycling | `v` | Inherited | Emulated VGA register/palette changes, not direct control of the Pi display DAC. |
| Correct 4:3 aspect ratios | `v` | Partial | LCD explicitly offers 4:3; CRT intentionally retains native raster and relies on physical geometry. |
| Integer scaling | `x` | Not integrated | No managed integer-scaling control. |
| Pixel-perfect rendering | `~` | Partial | Nearest-neighbor LCD/default native CRT rasters; pixel-perfect display sampling is not universally guaranteed. |
| CRT shaders | `x` | Not integrated | CRT appearance targets physical display hardware, without a managed shader pipeline. |
| Scanline effects | `x` | Not integrated | No software scanline effect controls. |
| Phosphor and shadow-mask simulation | `x` | Not integrated | No managed simulation/effect workflow. |
| VGA 70 Hz output | `v` | Implemented | Initial 720x400 request and refresh-aware KMS selection; requires suitable display/adapter. |
| CGA/EGA video timing | `v` | Inherited | Guest adapter timing plus host mode mapping; every physical timing is not validated. |
| Direct DRM/KMS output | `v` | Implemented | Native SDL2 KMS/DRM, no display server/compositor. |
| OpenGL ES acceleration | `~` | Implemented | SDL2 GLES presentation validated on Pi 5; not DOSBox desktop OpenGL or Glide passthrough. |
| VSync | `v` | Implemented | SDL presentation VSync enabled; distinct from emulated VGA vsyncmode. |
| Low-latency video output | `v` | Partial | Direct presentation avoids desktop layers; no universal measured input-to-photon latency claim. |
| Fullscreen mode | `v` | Implemented | Appliance fullscreen surface path, including first-frame KMS correction. |
| HDMI resolution switching | `v` | Implemented | CRT tracks rendered mode/rate; LCD deliberately keeps its selected fixed resolution. |
| Real CRT-compatible video modes | `v` | Implemented | Custom EDID/native-mode work; adapter/monitor electrical compatibility still matters. |
| 3dfx Voodoo emulation | `v` | Implemented | Software Voodoo preset enabled by default; performance and applications need validation. |

### Audio and Music

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| PC speaker emulation | `v` | Implemented | Independently enabled default, mixer control and short tone checks. |
| AdLib / OPL2 | `v` | Implemented | AdLib preset/FM backend; baseline OPL2 tone checks. |
| OPL3 | `v` | Inherited | Engine FM modes exposed where applicable; default SB Pro 2 is not an SB16 preset. |
| Sound Blaster | `v` | Implemented | SB 2.0 preset and resource controls. |
| Sound Blaster Pro | `v` | Implemented | SB Pro 2 is the current default at 220h/IRQ7/DMA1. |
| Sound Blaster 16 | `v` | Implemented | Optional preset/high DMA controls; used in short DMA tone validation. |
| Gravis UltraSound | `v` | Implemented | GUS Classic preset; required user patch files are not bundled. |
| Roland MT-32 emulation | `v` | Implemented | Munt profile requires supplied matching ROM pair. |
| General MIDI | `v` | Implemented | FluidSynth SoundFont profiles and independent MPU-401 configuration. |
| Roland Sound Canvas / SC-55 emulation | `v` | Partial | SC-55-named SoundFont profile, not established chip-accurate Sound Canvas hardware emulation. |
| SoundFont support | `v` | Implemented | Manifest-verified supplied SF2 assets staged outside Git. |
| External MIDI devices | `v` | Implemented | ALSA USB MIDI messages to external module; module audio is separate. |
| MIDI device selection | `v` | Implemented | Stable destination picker, rescan, ambiguity protection and bounded boot retry. |
| Audio latency configuration | `x` | Partial | Advanced mixer block/prebuffer INI values exist; no dedicated Pi-DOS latency UI/workflow. |
| Accurate audio timing | `v` | Partial | Engine scheduling and short checks; Automatic-profile MIDI problem and teardown XRUN remain. |
| Direct ALSA output | `v` | Implemented | HDMI/jack/USB selection without PulseAudio/PipeWire. |
| Audio/video synchronization | `v` | Partial | Mixer timing and presentation VSync exist; long sessions/demos and MIDI issue still need validation. |

### Storage and Filesystems

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| Persistent C: drive | `v` | Implemented | FAT32 PI-DOS volume root, writable/read-only handling and confirmed persistence. |
| Multiple DOS drives (C:, D:, etc.) | `v` | Implemented | Managed system/media drives plus free-letter USB/image/ZIP mappings. |
| DOS drive-letter mapping | `v` | Implemented | Protected C:/Z:, physical A:/D:, automatic USB allocation from E:. |
| FAT12 / FAT16 / FAT32 disk images | `~` | Inherited | Engine image support depends on format/geometry; physical floppy integration specifically uses FAT12. |
| Floppy disk images | `v` | Inherited | IMGMOUNT and multi-image switching retained. |
| Read-only disk images | `v` | Inherited | Engine read-only/media behavior; no general Pi-DOS disk-overlay guarantee. |
| Hard disk images | `v` | Inherited | IMGMOUNT/engine capability; not automatic host USB raw-disk exposure. |
| CD-ROM emulation | `v` | Implemented | Image MSCDEX support plus native physical USB CD adapter. |
| ISO image mounting | `v` | Implemented | IMGMOUNT on a free/released letter. |
| MSCDEX compatibility | `v` | Implemented | Engine interface extended by physical USB data/audio backend; game acceptance remains specific. |
| Multi-disc switching | `v` | Implemented | IMGMOUNT sets, IMGSWAP and configurable next-CD/next-floppy actions. |
| ZIP archive mounting | `v` | Implemented | ZIPMOUNT plus persistent sibling .PFS changes. |
| Host filesystem mapping | `v` | Partial | Managed C:/USB directory backends; arbitrary MOUNT paths/SUBST disabled in appliance mode. |
| Case-insensitive filenames | `v` | Implemented | DOS lookup semantics and case-insensitive sharing identity; host spelling retained. |
| 8.3 filename compatibility | `v` | Implemented | Classic uppercase aliases by default with optional long-name view/APIs. |
| DOS file attributes | `v` | Implemented | Native R/H/S/A on managed FAT/exFAT/floppy, checked persistence and ATTRIB recursion. |
| DOS timestamps | `v` | Implemented | Backend DOS timestamp handling/checked metadata commits within filesystem granularity. |
| DOS volume labels | `v` | Implemented | Protected C:/Z: labels and persistent checked floppy/USB labels, including FORMAT refresh. |
| USB storage mounting | `v` | Implemented | Automatic FAT12/16/32/exFAT, whole volumes/MBR/GPT, read-only fallback. |
| Automatic USB drive mapping | `v` | Implemented | Lowest free E:-upward letter; no saved assignments across boots. |
| Writable disk overlays | `x` | Partial | Writable persistent ZIP .PFS overlay implemented; no general image-overlay management feature. |
| File sharing and locking | `v` | Implemented | DOS sharing grants and 32-bit PSP-owned record locks inside one emulator. |
| Disk caching | `v` | Implemented | Floppy windows, FAT coherence and Linux caches; mutations retain checked completion semantics. |
| Persistent user configuration | `v` | Implemented | PIDOS.CFG and AUTOEXEC preservation, backup/temp save protocol. |

### Input and Controllers

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| USB keyboard | `v` | Implemented | Linux/SDL2/DOS path plus recorded physical keyboard operation/hotplug. |
| USB mouse | `v` | Implemented | SDL2 and INT33 path; later user hotplug record, broad physical application coverage limited. |
| DOS keyboard scan codes | `v` | Implemented | BIOS scan-code and modifier behavior checked with virtual input. |
| Mouse driver emulation | `v` | Inherited | INT33 driver provided by engine; setup mouse optional. |
| International keyboard layouts | `v` | Implemented | Searchable layout selection in System/welcome; independent of country/timezone. |
| Joystick emulation | `v` | Implemented | Stable global gameport type and configurable timed axes/autofire. |
| USB gamepads | `v` | Implemented | SDL mapping/raw fallback, stick/D-pad, button swap/dead zone. |
| Gamepad-to-keyboard mapping | `x` | Not integrated | No managed gamepad key-binding workflow. |
| Per-game controller profiles | `x` | Not integrated | Input configuration is global. |
| Hotplug support | `x` | Implemented | Keyboard/mouse device events, stable controller slots and automatic removable-media discovery. |
| Configurable keyboard shortcuts | `~` | Implemented | Six system actions with validated choices/duplicates and reboot application. |

### DOS and PC Hardware Compatibility

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| DOS INT 21h services | `v` | Inherited | Engine DOS API plus Pi-DOS attributes, sharing/locks and storage corrections. |
| BIOS INT 10h video services | `v` | Inherited | Guest video/text services; PIDOSCFG uses them for reliable buffered redraws. |
| BIOS INT 13h disk services | `v` | Inherited | Engine disk services extended for USB floppy sector/geometry access. |
| BIOS INT 16h keyboard services | `v` | Inherited | Guest BIOS keyboard interface, also used by PIDOSCFG. |
| BIOS timer services | `v` | Inherited | Guest timer services distinct from native Linux timers. |
| Programmable Interrupt Controller (PIC) | `v` | Inherited | Emulated PC interrupt controller. |
| Programmable Interval Timer (PIT) | `v` | Inherited | Emulated PC timer device. |
| DMA emulation | `v` | Inherited | Guest DMA machinery, including sound-card use. |
| VGA register emulation | `v` | Inherited | Guest register behavior; selected monitor profile affects physical presentation. |
| Keyboard controller emulation | `v` | Inherited | Virtual PC controller translates host input into guest behavior. |
| Hardware interrupt handling | `v` | Inherited | Guest interrupt model on top of the Linux-hosted emulator. |
| Direct I/O port emulation | `v` | Inherited | Emulated x86 port accesses, not arbitrary direct Pi hardware I/O. |
| PC hardware timing compatibility | `~` | Partial | DOS engine timing plus fixed profiles/native refresh work; not a cycle-accurate PC. |
| DOS device drivers (where compatible) | `~` | Partial | Compatible driver loading/WFWNET paths; arbitrary hardware drivers are not guaranteed. |
| Bootable FreeDOS/MS-DOS disk images | `~` | Partial | Inherited image boot route; user supplies media and validates configuration/drivers. |
| Alternative DOS shells such as 4DOS | `v` | Inherited | Engine alternative-shell facilities retained; no comprehensive Pi appliance acceptance. |
| Windows 3.x compatibility | `v` | Partial | Engine support and WfW networking helpers; user installation/media and physical checks required. |
| DOS networking and IPX | `~` | Implemented | IPX tunnel and NE2000 SLIRP/PCAP UI/startup; native ODI needs user drivers. |
| Serial port emulation | `v` | Inherited | Engine serial facilities; physical Pi serial mapping/use not product-validated. |
| Modem emulation | `x` | Not integrated | No managed modem configuration/acceptance workflow; inherited engine scope is separate. |
| BBS connectivity | `x` | Partial | mTCP TELNET enables compatible network endpoints; no dedicated modem/BBS setup workflow. |
| Multiplayer DOS gaming | `v` | Implemented | Optional IPX/NE2000 paths; particular games/peers require network acceptance. |
| Demoscene compatibility | `v` | Partial | Nine curated demos packaged; timing/music/playback not broadly certified. |

### Gaming and Convenience

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| Per-game configuration profiles | `x` | Not integrated | Profiles and PIDOS.CFG settings are global. |
| Automatic game configuration | `x` | Not integrated | Game setup remains explicit. |
| Game-specific CPU settings | `x` | Not integrated | Global CPU profiles only; no managed per-game persistence. |
| Automatic sound-card configuration | `v` | Partial | Startup selects/emits configured hardware environment; it does not configure every game's sound menus. |
| Save states | `x` | Not integrated | No supported Pi-DOS save-state workflow. |
| Load states | `x` | Not integrated | No supported Pi-DOS load-state workflow. |
| Rewind | `x` | Not integrated | No managed rewind facility. |
| Screenshots | `v` | Implemented | PNG captures, persistent folder and configurable shortcut. |
| Video recording | `x` | Not integrated | No managed Pi-DOS video recording path; upstream capture facilities are not a product claim. |
| Gamepad-friendly navigation | `x` | Not integrated | No gamepad navigation layer for setup/menus. |
| Automatic mounting of game directories | `x` | Not integrated | Managed drives and explicit image/ZIP mounts, without per-game automount. |
| ZIP game launching | `x` | Partial | Explicit ZIPMOUNT then run a program; no automatic archive launcher. |
| Automatic CD image selection | `x` | Not integrated | User chooses/mounts image sets and switches discs. |
| Multi-disc management (basic image switching; no saved disc lists) | `~` | Partial | Basic image cycling/selection implemented; no saved playlists or selector menu. |
| Optional graphical game launcher | `x` | Not integrated | DOS prompt/file manager/text content menus instead. |
| Optional DOS-style game browser | `x` | Not integrated | BENCH/DEMOS are curated menus, not a general game browser. |
| Game compatibility database | `x` | Not integrated | No maintained per-game compatibility catalog. |
| Game-specific performance optimizations | `x` | Partial | Specific correctness work such as Wolfenstein FPU fix, without a per-game optimization system. |

### Operating System and Boot

| NOTES.md feature | Original mark | Current status | Explanation / manual boundary |
| --- | --- | --- | --- |
| Boot directly to `C:\>` | `v` | Implemented | Normal writable startup; welcome may run first, missing storage yields Z: recovery. |
| No Linux desktop | `v` | Implemented | Appliance image omits desktop packages/interface. |
| No X11 or Wayland | `v` | Implemented | Native KMS/DRM platform; display servers disabled. |
| No systemd | `v` | Implemented | Custom C PID1 and explicit service supervision. |
| No login or user accounts | `v` | Implemented | No login/getty or normal multiuser workflow; runtime is root. |
| Root-only execution | `v` | Implemented | Supervisor requires root PID1; runtime uses root-owned hardware interfaces. |
| Minimal Buildroot system | `v` | Implemented | External tree with selected hardware/runtime dependencies. |
| Custom PID 1 | `v` | Implemented | C supervisor owns storage, runtime and shutdown. |
| Embedded initramfs | `v` | Implemented | Linux root is embedded in Image, not a separate root partition. |
| Read-only system image | `x` | Partial | Volatile root read-only remount attempted; BOOT/data are ordinary writable FAT partitions, not whole-image immutability. |
| Separate persistent DOS partition | `v` | Implemented | FAT32 PI-DOS partition supplies volume-root C:. |
| Native SDL2/SDL3 | `v` | Partial | Native SDL2 implemented; SDL3 deferred, not a simultaneous supported alternative. |
| Direct DRM/KMS | `v` | Implemented | SDL2 native fullscreen display path. |
| Direct ALSA | `v` | Implemented | Native ALSA output and sequencer without audio server. |
| Direct USB and input access | `v` | Implemented | Linux USB/HID/evdev and SCSI adapters; no desktop dependency. |
| Fast boot optimization | `v` | Partial | Quiet minimal startup/bounded waits; no current cross-board boot-time guarantee. |
| Minimal RAM consumption | `v` | Partial | No desktop/general-purpose OS layer; no universal measured RAM budget. |
| Minimal background processes | `v` | Implemented | Supervisor/runtime plus selected DHCP/Chrony and helper work; not literally one process. |
| Silent Linux initialization | `v` | Implemented | Splash/desktop absent and diagnostics primarily serial; errors/recovery still reported. |
| DOS-style startup screen | `v` | Implemented | Versioned BIOS-style hardware/configuration table and welcome. |
| DOS-style shutdown and reboot | `v` | Implemented | Commands/shortcuts/button route to native sync/unmount lifecycle. |
| Automatic hardware detection | `v` | Implemented | Board, display/audio/MIDI and media/controller discovery; selected-device policy still applies. |
| Raspberry Pi 3/4/5 ARM64 support | `v` | Implemented | Shared image and recorded boot/input on these families; full peripheral/model matrix remains open. |
| Reproducible SD card images | `v` | Partial | Pinned hashes and reproducibility controls; independent byte-for-byte rebuild not established. |
| Automatic DOS storage mounting | `v` | Implemented | Managed internal C: plus physical floppy/optical and USB volume registration. |
| Recovery from filesystem errors | `~` | Partial | Bounded validation/read-only/Z: fallback; no automatic repair or power-loss guarantee. |
| Optional serial debugging | `v` | Implemented | Kernel/supervisor serial diagnostics and documented temporary HDMI diagnostic path. |
| Offline operation without networking services | `v` | Partial | Offline DOS use works without connectivity, but current image starts DHCP/Chrony; no service-free profile UI. |

## 18. Complete development history

Every commit reachable from the reviewed original main branch is listed below.
The hashes identify the original local history. The active repository uses
cleaned history with the local deployment configuration removed from every
commit, so its commit IDs differ; subjects/order preserve traceability.
Earlier defaults, paths and claims are historical rather than current settings.

| Original commit | Date | Change | Technical/user-visible significance |
| --- | --- | --- | --- |
| `8fb1e69` | 2026-10-24 | initial commit baseline | Established the project baseline: pinned native SDL2/DOSBox-X development tooling, ARM64/image architecture, C supervisor, Buildroot external tree, persistent storage/image tooling, validation fixtures/reports and board boot assets. Later commits refine this baseline rather than introducing an unrelated product. |
| `a391e08` | 2026-10-24 | Improve native CRT video modes and make diagnostics optional | Preserved DAC modes, added VGA/EGA/Hercules-sized timings, matched KMS rendered dimensions and guest refresh, embedded EDID on both HDMI ports and made video tracing optional. Later unified logging supersedes its separate flag. |
| `7bc272b` | 2026-10-24 | Fix HDMI audio by upgrading the custom EDID to version 1.3 so Linux detects its audio capabilities, and pin output to the primary HDMI0 port. | Promoted custom EDID to 1.3 so Linux recognizes HDMI audio capability and selected primary HDMI0 audio. This addresses display capability parsing and output routing. |
| `67f7ae7` | 2026-10-24 | Enable SDL presentation VSync to fix periodic scrolling judder | Enabled SDL presentation VSync to address periodic scrolling judder. Guest VGA vsyncmode remains a distinct setting. |
| `38516c9` | 2026-10-25 | Expose persistent DOSBox configuration and bundle ARJ on Z: | Introduced persistent emulator configuration and bundled the supplied ARJ utility on Z:. Current PIDOS.CFG naming follows later configuration/branding work. |
| `fed33a8` | 2026-10-25 | Add standard DOS folders, Doszip Commander and user startup template | Added standard writable DOS folders, Doszip/DZ, a writable personalized editor path, TEMP/TMP and seed-only user AUTOEXEC customization. |
| `adda032` | 2026-10-25 | Simplify drive layout and apply meaningful volume labels | Simplified drives, fixed DOS C:/Z: labels to SYSTEM/PI-DOS, required PI-DOS host-volume identity and freed D: from its old default data role. |
| `b5214ed` | 2026-10-25 | Add a shared ARM64 image for Raspberry Pi models | Created one Cortex-A53-baseline ARM64 image with common 4 KiB pages, multi-model DTBs/overlays/firmware, shared board assets and model-aware primary HDMI audio. |
| `4f5c567` | 2026-10-25 | Add opt-in diagnostics and fix Raspberry Pi 4 USB startup | Added opt-in boot/runtime diagnostics and pinned matched firmware correcting Pi 4 USB startup timeouts; later commits replace the old diagnostic flag. |
| `19aad27` | 2026-10-25 | Rename remaining ARM-DOS references to Pi-DOS | Completed ARM-DOS to Pi-DOS naming changes across runtime, packages, tools and documentation, and corrected clean Buildroot extraction. Historical config filenames are not the current user interface. |
| `a4a4458` | 2026-10-26 | Fix ARM64 FPU register aliasing corrupting Wolfenstein 3D | Fixed overlapping x87 register view aliasing on optimized ARM64, repairing Wolfenstein trigonometric table corruption with interpreter and dynrec; added focused regression evidence. |
| `d220df9` | 2026-10-26 | Fix boot info table borders with Spanish keyboard layout | Changed boot-table border characters to values shared by CP437/CP850 so Spanish layout/code-page changes do not corrupt junctions. |
| `21c232c` | 2026-10-26 | Add network time synchronization and regional configuration | Added wired DHCP/Chrony, host-to-DOS clock synchronization, PIDOS.CFG with a Pi-DOS section, configurable timezone/DST and Spain regional defaults. |
| `475aed0` | 2026-10-26 | Unify diagnostic logging under PIDOS.CFG | Consolidated boot/runtime/network/video log control under [pidos] diagnostics; removed separate kernel/video flags and aligned image defaults/validation. |
| `9e3e50b` | 2026-10-26 | Show local date and time on boot and default to Spanish keyboard | Displayed local date/time during boot and made the default DOS keyboard Spanish. |
| `82fb574` | 2026-10-26 | feat: add DOS CPU profile setup and default to Pentium 133 MHz | Added standalone DOS CPU setup, profile definitions, page defaults, config backup and CPU boot summary. Its Pentium 133 default is superseded. |
| `2ec2932` | 2026-10-27 | feat: extend Pi-DOS setup with hardware, MIDI and system settings | Expanded setup to memory, video/Voodoo, sound resources, internal/external MIDI, live mixer, regional and diagnostic controls; integrated boot summary and assets. |
| `6bbb85c` | 2026-10-27 | chore: bump Pi-DOS version from 0.1 to 1.0 | Bumped project version from 0.1 to 1.0; version branding is separate from DOS compatibility reporting. |
| `464942d` | 2026-10-27 | fix: route Ctrl+Alt+Delete through the supervised reboot path | Made Ctrl+Alt+Delete request a synchronized whole-Pi reboot through PID 1, including keypad Delete and repeat suppression, rather than a guest-only reset. |
| `5b3035f` | 2026-10-27 | feat: support clean shutdown with the Pi 5 power button | Added native Pi 5 power-button handling, supervised shutdown marker and confirmation-dialog bypass so normal runtime teardown can complete during a game. |
| `88a1aca` | 2026-10-27 | fix: mark Sound Blaster 16 as default in PIDOSCFG | Corrected the then-current SB16 default indication in setup. Current source defaults are later SB Pro 2 and must take precedence over the historical label. |
| `4b352da` | 2026-10-27 | Add LCD/CRT monitor profiles and default CPU to Automatic | Added fixed LCD/native CRT profiles, LCD resolution/aspect settings and legacy-config preservation. Its Automatic CPU default is superseded by Pentium 120. |
| `805115f` | 2026-10-28 | Fix USB MIDI detection race during boot | Moved saved USB MIDI resolution after display/audio discovery and retried for up to five seconds to survive enumeration races. |
| `006f3d9` | 2026-10-28 | Add native USB floppy support as DOS drive A: | Added native automatic A:, BIOS/raw sector and FAT12 access, disk/write-protect changes, DPB/FORMAT corrections, checked commits, transfer/cache optimizations, responsive I/O and kernel polling cap. Commit records Pi 5 Toshiba/TEAC copy and full/quick-format confirmation. |
| `2c17155` | 2026-10-28 | Add automatic USB optical drive support on D: | Added native automatic D: with MSCDEX data/mixed-mode/digital CD audio, hotplug/media-change handling, bounded I/O and stale-handle protection. |
| `cf62421` | 2026-10-28 | Add VIDEOMOD command to list host and DOS video modes | Added VIDEOMOD host and BIOS/VESA listing with /HOST, /DOS, /P and help. |
| `6ab5f8b` | 2026-10-29 | feat(storage): automount USB volumes and manage DOS drive assignments | Added FAT/exFAT USB data automount/free letters, synchronous responsive writes, volume-root managed C:, protected drive policy, classic 8.3 presentation and not-ready media errors. |
| `383934c` | 2026-10-29 | Persist DOS volume labels on floppy and USB storage | Persisted floppy/USB FAT/exFAT volume labels, refreshed labels after FORMAT and added checked flush/unmount/relabel/remount behavior with locale/encoding support. |
| `05bdc46` | 2026-10-29 | Implement native DOS file attributes across storage drives | Implemented native R/H/S/A persistence/enforcement on managed media, archive-bit updates, search behavior and recursive/error-aware ATTRIB coverage. |
| `18c0b09` | 2026-10-29 | Implement DOS file sharing and record locking | Implemented sharing-mode checks, PSP-owned 32-bit record locks, destructive-operation protection, inherited/duplicate handles, cleanup and cross-handle cache coherence. |
| `a7b35a8` | 2026-10-29 | Add ZIP mounting with persistent .PFS overlays | Added ZIPMOUNT, per-archive persistent .PFS copy-up, deletion/rename semantics, read-only views and independent overlays while preserving managed letters. |
| `c499af4` | 2026-10-29 | Add controller hotplug and configurable system shortcuts | Added Input page/global controller options, normalized/raw gamepad hotplug with stable slots, configurable system shortcuts and virtual-file-position correction for built-in loader error 33. |
| `1a8c287` | 2026-10-29 | Default to CRT output and add optional SDL2 linear filtering | Made CRT the fresh/restored monitor default, added optional LCD linear filtering and fixed native FAT volume-root listings. Nearest-neighbor filtering remains default. |
| `0fb768a` | 2026-10-29 | Enable DOS networking and add PIDOSCFG Network tab | Enabled compiled IPX/SLIRP/PCAP support, added Network controls/generated startup, packet-driver/mTCP bundle, native-IPX user-driver path and WFWNET helpers. |
| `36e9661` | 2026-10-30 | feat: add screenshots and configurable disc-switching shortcuts | Added persistent PNG captures and configurable next-CD/next-floppy image-set actions, plus System-page scrolling. Physical media are excluded from image cycling. |
| `e4e3cf9` | 2026-10-30 | Remove 15 unused and redundant bundled DOS utilities | Removed fifteen redundant/unwanted Z: commands and standalone payloads while preserving EDIT/FC and independent PC-98 CWSDPMI registration. |
| `f43010a` | 2026-10-30 | Add audio output selector to PIDOSCFG System settings | Added saved HDMI/jack/USB DAC selection, bounded ALSA discovery, analogue kernel/firmware support and no-fallback behavior for unavailable selected output. |
| `001f78e` | 2026-10-30 | Add DOS benchmark pack to Pi-DOS images | Added curated benchmark payload/menu, original shareware installers/notices and inventory verification; corrected Quake argument case and mode-before-timedemo ordering. |
| `6be9759` | 2026-10-30 | Add curated DOS demoscene pack with text launcher | Added nine checksum-pinned original demoscene releases with numbered text launcher, setup notes, retained archives/inventory and standalone make demos ZIP target. |
| `6f39d63` | 2026-10-30 | feat: add first-boot setup and refresh CPU profiles | Added two-stage welcome/keyboard/clock setup and refreshed CPU presets to Pentium 75/120/MMX166, making Pentium120 default. Regional/display changes reboot before review. |
| `ff0c194` | 2026-10-30 | fix: prevent black first-boot welcome on KMS displays | Re-presented first KMS surface for static welcome, preserved text mode and BIOS-buffered setup redraws, embedded project version in titles and fixed volume-root VIDEO.LOG path. |

### Source precedence and supporting references

Current behavior was reconciled against `configs/pi-dos.conf`, the Buildroot
recipes/kernel/boot files, `src/init`, `src/common`, `src/doscfg`, `src/runtime`,
the maintained patch series, asset manifests and all commit messages/diffs.
Historical and validation references are retained for their specific evidence:

- [Image architecture, storage, boot and detailed procedures](buildroot-pi5.md)
- [Original proposed architecture (not a current feature guarantee)](technical-plan.md)
- [Native SDL2 baseline and acceptance checklist](phase0.md)
- [Native Pi5 execution evidence](pi5-validation.md)
- [GPU/KMS validation](pi5-graphics-validation.md)
- [Virtual keyboard path](pi5-keyboard-validation.md)
- [Short audio validation and teardown limitation](pi5-audio-validation.md)
- [Wolfenstein x87 investigation](wolf3d-corruption.md)
- [DOS setup implementation and controls](../src/doscfg/README.md)
- [Networking procedures](networking.txt)
- [DOSBox-X patch inventory](../patches/dosbox-x/README.md)
- [DOS payload provenance](../br2-external/board/raspberrypi/dos/README.md)
- [Component notices and unresolved release review](../LICENSES/README.md)
- [Repository synchronization and cleaned history](github-sync.md)
