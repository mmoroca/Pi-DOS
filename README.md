# Pi-DOS

Pi-DOS is a dedicated DOS-compatible environment targeting Raspberry Pi models
with 64-bit Arm processors. A minimal Linux system supplies hardware services;
the DOSBox-X-derived DOS environment is the only intended user interface.

The architecture is Buildroot, an embedded initramfs, a small C PID 1 supervisor,
native SDL2 KMS/DRM video, ALSA audio, USB input and persistent DOS storage. The
runtime runs as root. Pi 5 supplied the first hardware validation baseline.

**Status: shared-image boot and keyboard operation are user-confirmed on Pi 3/Pi 5, and on Pi 4 after the firmware correction below. Earlier Pi 5 persistence, reboot and poweroff checks passed; broader hardware validation remains open.**
The user confirmed boot/input on Pi 3 and Pi 5 with the shared image. On
25 October 2026, replacing the Pi 4's matched `start4.elf`/`fixup4.dat` pair
with the pinned May 2026 firmware restored USB input and removed its roughly
17-second controller timeout. Future builds use that firmware pin; other-board
coverage still needs hardware validation.
The SDL2 baseline builds and runs the small DOS fixtures on x86-64 and Pi 5.
The Pi's ARM64 dynrec execution was confirmed with GDB. Hardware GLES rendering,
4:3 VGA output and KMS page flips now pass with a small display-dimension patch.
The virtual Linux keyboard → SDL2 → DOS input test also passes over SSH.
PC speaker, AdLib and Sound Blaster tones pass the short ALSA HDMI playback
check; a teardown underrun is recorded separately. Mouse input, audible
speaker output and broader compatibility validation remain open.
See the [technical plan](docs/technical-plan.md) and
[baseline findings and acceptance checklist](docs/phase0.md).

## Build the shared Raspberry Pi ARM64 image

```sh
make image-config
JOBS=4 make image
```

For troubleshooting without a serial adapter, run `JOBS=4 make image-diagnostic`.
The separate `pi-dos-arm64-diagnostic-sdcard.img` enables all four logs in
`C:\SYSTEM`: `BOOT.LOG`, `RUNTIME.LOG`, `NETWORK.LOG` and `VIDEO.LOG`.
On either image, edit `C:\SYSTEM\PIDOS.CFG` from DOS and set `diagnostics=true`
or `diagnostics=false` under `[pidos]`, then reboot. Normal images default to
`false`. Logs are replaced on each enabled boot. See the
[diagnostic procedure](docs/buildroot-pi5.md#diagnostic-image-with-logs-on-c).

The initial 769 MiB development image is
`output/buildroot-arm64/images/pi-dos-arm64-sdcard.img`. It contains an ARM64 kernel
with embedded initramfs, custom C PID 1, the SDL2 DOS runtime, FAT32 BOOT and
FAT32 PI-DOS partitions for direct access from modern Windows. The generated image passed static checks for its
partitions, required kernel/SDL backends, runtime dependencies and absence of
a Linux shell or conventional init system.

The shared image targets Pi 2B revision 1.2, Pi 3B/3B+/3A+, Zero 2 W,
Pi 4B/400, Pi 5/500/500+, and the corresponding 64-bit Compute Modules
(CM0, CM3/3+, CM4/4S and CM5/Lite with suitable carrier hardware). Original
Pi 1, Zero/Zero W and Pi 2B revision 1.1 are 32-bit and cannot run it.
The build uses a Cortex-A53 baseline, a common 4 KiB-page kernel, per-model
device trees and boot firmware. Hardware validation of this shared image is
still required; the earlier Pi 5 results do not establish other-model support.
Use the HDMI output and wired USB input; Zero/Compute Modules need suitable
USB host adapters/carriers. Multi-model builds use a fresh `output/buildroot-arm64`
directory, without reusing Cortex-A76 binaries from `output/buildroot-pi5`.

The `PI-DOS` data partition automatically expands to the remaining SD-card space before DOS starts
on first boot. Subsequent boots skip expansion once the partition and filesystem
fill the card. The FAT32 resizer passed host tests on sparse 2 GiB and 64 GiB
card images; this updated image still needs ARM64 and physical SD-card testing.
Storage detection requires the `PI-DOS` volume label; existing FAT32 cards
labelled `DOSDATA` must be relabelled before using the updated system.

The default DOS PATH includes all bundled utility directories.
DOS volume labels default to `SYSTEM` for C: and `PI-DOS` for Z:.
C: uses the data partition's available space. D: is reserved for an automatic
USB optical drive; CD/DVD/Blu-ray units can read regular CDs, including digital
CD audio and mixed-mode game discs. C: exposes the PI-DOS volume root directly;
`SYSTEM`, `PROGRAMS`, `GAMES`, `DOCS` and `TEMP` are at that root.
Host checks pass; physical optical-drive validation is pending. See
[USB CD-ROM support](docs/buildroot-pi5.md#usb-optical-drive-d).
USB floppy drives automatically use `A:`, including when connected after boot.
Use `DIR A:`, `COPY`, or `FORMAT A:` normally; disk changes and write protection
are handled on access. Reads are batched by track. Physical drive validation is pending;
see [USB floppy support](docs/buildroot-pi5.md#usb-floppy-drive-a).
The image includes a personalized editor at `C:\PROGRAMS\TOOLS\EDIT.COM`; this
writable copy takes priority over the bundled editor and retains saved preferences.
Edit `C:\SYSTEM\PIDOS.CFG` and reboot to change emulator settings persistently.
Fresh images use the Pentium 133 MHz CPU profile (`pentium`, `dynamic_rec`,
`fixed 71240`); PIDOSCFG's CPU Restore defaults selects the same profile.
PIDOSCFG's System page selects CRT (the default) or LCD output.
CRT follows DOS video modes. LCD offers manual resolutions (1024x768 by default),
4:3 or Original pixels aspect, and optional SDL2 linear filtering (off by default).
Save and reboot to apply. Existing configurations keep their display behavior
until a monitor type is selected. See the [monitor controls](src/doscfg/README.md#monitor-output).
Type `VIDEOMOD /P` to list host display and DOS BIOS/VESA modes a screen at a time.
Use `/HOST` or `/DOS` to list either group, or `VIDEOMOD /?` for help.
The same file accepts `[pidos]` options, including `timezone=Europe/Madrid` for
local time with automatic daylight saving.
The default keyboard layout is Spanish (`keyboardlayout=sp` under `[dos]`).
PIDOSCFG's Input tab selects DOS joystick hardware, automatic/raw controller
layout, stick/D-pad controls, button order and dead zone. Controller slots survive
other-device removal and accept connections after boot, without mapper files.
The System tab configures clean reboot/poweroff and pause shortcuts. Save and reboot
to apply. Host virtual-controller checks pass; physical controller hotplug and
in-game calibration still need validation. See [input options](src/doscfg/README.md#input-and-controllers).
The boot summary displays the system date and local time;
network time synchronization may correct the clock after startup.
Edit `C:\AUTOEXEC.BAT` to customize environment variables, PATH or programs run
after standard startup. A minimal template is created only when missing;
existing files are preserved. `C:\README.TXT` explains both configuration files.
Pi-DOS owns C: and Z:. `MOUNT` lists drives and can eject removable drives;
additional directory mounts and `SUBST` mappings are disabled in appliance mode.
Use `IMGMOUNT` with a free letter for disk or CD images. The old `c/` layout is
not migrated automatically; its contents must be placed at the volume root.
See [managed drives](docs/buildroot-pi5.md#managed-drive-assignments-and-the-c-root).
Use `ZIPMOUNT E C:\GAMES\DOOM.ZIP` with a free letter to run files directly
from a ZIP. Changes persist in the ordinary sibling folder `DOOM.PFS`; the
archive stays unchanged. Add `-ro` to disable writes, and use `ZIPMOUNT E -u`
to unmount. See [ZIP drives](docs/buildroot-pi5.md#zip-archive-drives).
The shell shows uppercase 8.3 names by default; `DIR /-Z` shows long names.
Original filenames on disk and long-filename access are preserved.
The original Z: utility layout is retained, with the supplied ARJ at `Z:\BIN\ARJ.EXE`.
Applications live under `C:\PROGRAMS`. Doszip Commander is included in
`C:\PROGRAMS\DOSZIP`; type `DZ` to open its two-panel file manager.
The standard user folders are `C:\GAMES`, `C:\DOCS` and `C:\TEMP`.
Writable boots set both `TEMP` and `TMP` to `C:\TEMP`.
SDL2 KMS/DRM teardown keeps the host mouse pointer hidden during reboot and poweroff;
visual confirmation on hardware is pending.

Moving from the old ext4 image requires backing up `c` and `d`, flashing the
new image, and restoring files into the PI-DOS volume root (including any former D: files).
A kernel-only update does not convert
the filesystem. The build does not flash any physical card.

This is the first operating-system packaging work beyond the earlier native
DOSBox-X baseline. Boot to the DOS prompt and wired USB keyboard input,
including reconnection, are confirmed on the test Pi. The user also confirmed
C: file persistence across `REBOOT` and successful `POWEROFF`. The ReplayOS
development Pi has not been flashed or rebooted. This image has no SSH server.
Mouse validation and the audio teardown underrun are deferred as requested.
See [image build details and the first boot procedure](docs/buildroot-pi5.md).

## Build the native baseline

On a native Linux development host, install Python 3.12+, a C/C++ compiler,
GNU Make, GNU Patch, Autoconf, Automake, pkg-config, CMake and Ninja, plus development
headers/libraries for ALSA, libdrm, GBM, EGL and OpenGL ES. Additional dependencies
reported by the pinned DOSBox-X configuration may be needed.

```sh
make fetch
make sdl2   # independently build the platform dependency
make phase0
make smoke  # minimal headless DOS shell startup check
make smoke-programs  # COM/EXE/BAT and basic file operations; requires NASM
# On the Pi over SSH, with an unused connected HDMI display:
make smoke-graphics  # GPU readback, color/aspect checks and KMS presentation
make smoke-keyboard  # root: temporary uinput keyboard, BIOS/DOS key validation
make smoke-audio  # DOS tones, ALSA WAV capture and real HDMI playback status
# With archives already cached:
bash tools/phase0/build.sh --offline
```

Use `JOBS=2 make phase0` on a memory-constrained host. The workflow builds a private
SDL2 and then configures DOSBox-X with `--enable-sdl2`. It does not install system
packages or write outside this project. Ordinary development builds do not need
root. The eventual appliance runtime is root-only.

Sources and SHA-256 checksums are in [sources.lock.json](sources.lock.json).
The selected SDL2 is **native SDL 2.32.10**, built from the upstream SDL repository.
It does not use `sdl2-compat` or SDL3.
These pins are candidates until the hardware acceptance gates pass. Source
pinning alone does not establish bit-for-bit build reproducibility. The build
also applies the tracked [DOSBox-X patch series](patches/dosbox-x/README.md).

SDL3 is deferred until DOSBox-X has a complete, validated native SDL3 port.
The [earlier SDL3 findings](docs/phase0-sdl3-history.md) are retained for that
future upgrade. See the [current findings](docs/phase0.md) for build results
and remaining acceptance work, including the [Pi 5 validation report](docs/pi5-validation.md).

Generated paths:

| Path | Contents |
| --- | --- |
| `dl/` | Checksum-verified source archives |
| `output/src/` | Extracted upstream sources and generated Autotools files |
| `output/host-sdl2/` | Private native SDL2 installation |
| `output/build/sdl2/` | Native SDL2 build |
| `output/build/dosbox-x-sdl2/` | Native DOSBox-X build |
| `output/logs/` | Configuration and compilation logs |

An x86-64 build can check native SDL2 integration. Native ARM64 execution and
dynrec have now passed the small fixtures on Pi 5; Raspberry Pi display, input,
audio and boot behavior require separate hardware validation.

For the immediate startup check, run `make smoke` from this directory. It uses
SDL2 dummy video/audio and default settings, mounts a fresh scratch directory as
`C:`, writes a result from the DOS shell and exits. A visible DOS window is not
expected. Running the binary without these options can load a personal DOSBox-X
configuration requesting unavailable X11 or Wayland backends.

After `make smoke` passes, run `make smoke-programs`. It assembles two original
DOS fixtures with NASM, then runs them through a BAT script using the normal
interpreter. It checks COM execution, an MZ EXE relocation, child exit codes,
batch variable expansion and DIR/CD/COPY. Results and logs stay in a fresh
directory under `output/smoke/`. Both headless checks have passed on x86-64 and
Pi 5 ARM64. To repeat the Pi's dynrec execution check with GDB:

```sh
CPU_CORE=dynamic_rec VERIFY_DYNREC=1 make smoke-programs
```

This requires GDB and debug symbols. The debugger breakpoints are tied to the
pinned DOSBox-X revision; see the Pi 5 report for details and scope.

The Pi is a shared ReplayOS development environment accessed through SSH only.
Pi tests use scripted commands and automatic exit; local console interaction is
not required. The [graphics check](docs/pi5-graphics-validation.md) captured the
VGA pattern from the V3D GPU and observed completed KMS flips. It exits
automatically and refuses to run when DRM card devices are already in use.
The [keyboard check](docs/pi5-keyboard-validation.md) verifies BIOS scan codes,
characters and Shift handling through Linux uinput and SDL2 evdev, then removes
its virtual device. The [audio check](docs/pi5-audio-validation.md) validates
three DOS tones in an ALSA capture and advancing real HDMI hardware playback.
Physical input and audible speaker output still need separate evidence.

No proprietary DOS binaries or applications are included. Project licensing is
not yet selected; upstream component notices remain applicable. See
[source provenance](LICENSES/README.md).
