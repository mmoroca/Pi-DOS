# Pi-DOS

Pi-DOS is a proposed dedicated DOS-compatible environment for Raspberry Pi
3B/3B+, 4B and 5B using AArch64. A minimal Linux system supplies hardware services;
the DOSBox-X-derived DOS environment is the only intended user interface.

The architecture is Buildroot, an embedded initramfs, a small C PID 1 supervisor,
native SDL2 KMS/DRM video, ALSA audio, USB input and persistent DOS storage. The
runtime will run as root. Pi 5 is the first target.

**Status: the previous Pi 5 image passed boot, keyboard/reconnection, persistence, reboot and poweroff checks. The new FAT32 image awaits hardware retesting.**
The SDL2 baseline builds and runs the small DOS fixtures on x86-64 and Pi 5.
The Pi's ARM64 dynrec execution was confirmed with GDB. Hardware GLES rendering,
4:3 VGA output and KMS page flips now pass with a small display-dimension patch.
The virtual Linux keyboard → SDL2 → DOS input test also passes over SSH.
PC speaker, AdLib and Sound Blaster tones pass the short ALSA HDMI playback
check; a teardown underrun is recorded separately. Mouse input, audible
speaker output and broader compatibility validation remain open.
See the [technical plan](docs/technical-plan.md) and
[baseline findings and acceptance checklist](docs/phase0.md).

## Build the Pi 5 image

```sh
make image-config
JOBS=4 make image
```

The initial 769 MiB development image is
`output/buildroot-pi5/images/armdos-pi5-sdcard.img`. It contains an ARM64 kernel
with embedded initramfs, custom C PID 1, the SDL2 DOS runtime, FAT32 BOOT and
FAT32 DOSDATA partitions for direct access from modern Windows. The generated image passed static checks for its
partitions, required kernel/SDL backends, runtime dependencies and absence of
a Linux shell or conventional init system.

DOSDATA automatically expands to the remaining SD-card space before DOS starts
on first boot. Subsequent boots skip expansion once the partition and filesystem
fill the card. The FAT32 resizer passed host tests on sparse 2 GiB and 64 GiB
card images; this updated image still needs ARM64 and physical SD-card testing.

The default DOS PATH includes all bundled utility directories.
The image includes a personalized editor at `C:\SYSTEM\EDIT.COM`; this
writable copy takes priority over the bundled editor and retains saved preferences.
SDL2 KMS/DRM teardown keeps the host mouse pointer hidden during reboot and poweroff;
visual confirmation on hardware is pending.

Moving from the old ext4 image requires backing up `c` and `d`, flashing the
new image, and restoring those files. A kernel-only update does not convert
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
