# Pi-DOS — Technical implementation plan

Version 1.1 · 21 October 2026 · Proposed architecture

Revision 1.1 selects native SDL2 for the initial release. Upgrade to SDL3 only
after DOSBox-X provides a complete native SDL3 port and the Pi-DOS integration
and hardware gates pass. The initial SDL3 investigation is retained separately.

This document records the supplied architecture and its development gates.
It describes intended behavior, not implemented functionality. Current evidence
belongs in [phase0.md](phase0.md).

## Objective and scope

Boot Raspberry Pi models with 64-bit Arm processors directly to a DOS prompt,
including Pi 2B rev 1.2, Pi 3/4/5 families, Zero 2 W and their 64-bit Compute
Modules. Use one Cortex-A53-baseline image with model-specific boot assets. Execute
x86 COM and EXE applications and BAT scripts using DOSBox-X CPU, DOS, BIOS,
memory and virtual hardware services. Provide persistent DOS drives, VGA,
keyboard, mouse, PC speaker, AdLib and Sound Blaster audio.

Linux is an internal hardware layer. Initial releases exclude a desktop, Linux
shell or login, general-purpose services, networking, remote administration,
multiuser operation, Windows 9x, cycle-accurate PC emulation and a bare-metal
ARM kernel. Native runtime components execute as root.

## Architecture

1. **Hardware:** Raspberry Pi ARM64 CPU, display/GPU, USB, HDMI/audio, timers and
   SD or supported mass storage.
2. **Kernel:** Minimal Linux for memory, scheduling, interrupts, timers,
   DRM/KMS, ALSA, evdev/HID, block devices and filesystems.
3. **Native platform:** Custom C init, configuration, SDL2, drive management,
   hardware detection, supervision and shutdown/reboot.
4. **DOS VM:** DOSBox-X-derived x86 emulator, ARM64 dynrec and interpreter,
   virtual DOS kernel, BIOS, shell, PC hardware and DOS filesystem services.

Keep board-specific code outside the DOS engine. Preserve upstream history when
establishing the DOSBox-X fork; organize engine changes as a small patch series.
Avoid removing upstream functionality before establishing a working reference.

## System image

Use a Buildroot external tree rather than a Buildroot fork. Pin Buildroot,
Linux, SDL2, DOSBox-X and all image inputs after validation. Produce kernel,
DTBs, embedded initramfs, runtime libraries, firmware and a bootable SD image.

Planned Buildroot choices:

```text
BR2_INIT_NONE=y
BR2_ROOTFS_DEVICE_CREATION_DYNAMIC_DEVTMPFS=y
BR2_TARGET_ROOTFS_CPIO=y
BR2_TARGET_ROOTFS_INITRAMFS=y
```

Start from working Raspberry Pi kernel configurations and trim incrementally.
Retain ARM64, SMP/MMU, executable mappings, high-resolution timers, ELF,
initramfs, devtmpfs/procfs/sysfs, FAT, ext4, SD/MMC, USB HID/evdev, DRM/KMS,
VC4/V3D as required, ALSA and Pi 5 PCIe/RP1. Disable unused hardware, filesystems,
network protocols, Wi-Fi, Bluetooth and virtualization. Do not weaken essential
memory protection, scheduling, interrupts or timers to reduce size.

Use a common 4 KiB kernel page size across all supported boards. Validate 16 KiB
separately before any model-specific optimization, including JIT cache and
protection behavior.

The system image is immutable during normal operation. Writable DOS data lives
on a separate FAT32 partition; temporary state is memory-backed where possible.
There is no writable conventional Linux root filesystem, BusyBox requirement,
shell, service manager, login manager or SSH daemon.

## PID 1 and lifecycle

Implement `/init` in C and keep it as PID 1 throughout the runtime. It mounts
virtual filesystems, prepares runtime directories, discovers DOS storage,
configures SDL2, launches and supervises the emulator, reaps children, and
coordinates shutdown/reboot and storage synchronization.

Missing or damaged storage must produce a bounded, usable failure path rather
than an indefinite boot wait. Do not silently write persistent data into the
initramfs when the data partition is unavailable. Define the discovery timeout,
read-only recovery behavior and runtime-to-supervisor shutdown protocol before
implementing the image. Avoid automatic repair delays during normal boot.

Logical runtime paths are `/init`, `/pi-dos/pi-dos`, `/pi-dos/pi-dos.conf`,
`/lib`, `/dev`, `/proc`, `/sys`, `/run` and `/dosdata`. Include the actual dynamic
loader, libc, linked dependencies, Mesa drivers and required firmware.

## Native SDL2 platform

Use native SDL2, built from the SDL2 release sources, without `sdl2-compat` or an
SDL3 runtime. Force `SDL_VIDEODRIVER=kmsdrm` and `SDL_AUDIODRIVER=alsa` in the
appliance. These are SDL2's environment variable names.
No X11, Wayland, compositor, PulseAudio, PipeWire or audio service daemon.

The intended fullscreen rendering path is VGA framebuffer → SDL2 texture →
OpenGL ES → KMS/DRM → HDMI. Preserve DOS pixels and aspect ratios, support VSync,
and avoid unnecessary shaders. A fixed HDMI mode is acceptable initially.
Building the SDL2 dependency alone does not demonstrate this rendering path;
the DOSBox-X output backend must also use GLES in the hardware validation gate.

Reuse VGA, CGA/EGA and text-mode emulation. Implement a dedicated platform output
backend only after baseline validation, without retaining every upstream output
mode. Linux-specific graphics/input access belongs behind the platform layer.

Reuse PC speaker, OPL2/AdLib and Sound Blaster/SB16 emulation with stable SDL2
ALSA output and A/V timing. Translate USB keyboard/mouse input with DOS scan
codes, layouts, modifiers, extended keys and Ctrl+C/Ctrl+Break behavior. Start
with evdev; explicitly test enumeration and hotplug without a permanent udev
daemon before deciding whether a small discovery helper is required.

## DOS execution and compatibility

Reuse the DOSBox-X DOS shell, DOS/BIOS services and x86 emulator. Support
8086/8088, 286, 386, 486 and upstream Pentium modes where available. Retain real
and protected mode, paging, x87, interrupts, self-modifying code and extenders.

Prefer ARM64 dynrec with an interpreter fallback. Validate instruction generation,
instruction-cache invalidation, executable memory/protection, code-cache
synchronization, host page sizes and fault handling. Diagnose differences against
the interpreter. Compilation on x86-64 cannot establish ARM64 correctness.

Reuse DOS INT 20h/21h, PSP/processes, files/directories, handles, environment,
date/time, allocation and errors; BIOS INT 10h/13h/16h/1Ah; PIC/PIT/DMA/VGA,
keyboard and audio devices. Provide conventional 640 KiB memory, upper memory,
EMS, XMS, HMA and DPMI as supported. PC BIOS services always refer to virtual
hardware, never Raspberry Pi peripherals directly.

The initial shell supplies DIR, CD/CHDIR, MD/MKDIR, RD/RMDIR, COPY, DEL, REN,
TYPE, CLS, PATH, SET, ECHO, VER and MEM, DOS parsing, BAT execution and variable
expansion. Startup resembles AUTOEXEC.BAT. Document unsupported CONFIG.SYS
directives and x86 drivers rather than asserting complete MS-DOS compatibility.

## Persistent storage and host isolation

| Partition | Filesystem | Purpose |
| --- | --- | --- |
| BOOT | FAT32 | Pi firmware, kernel and device trees |
| PI-DOS | FAT32 | DOS files and user configuration |

Map `C:\` to `/dosdata` and reserve `D:\` for a USB optical drive reading regular
data/audio CDs, including mixed-mode games. CD/DVD/Blu-ray units are eligible;
DVD/Blu-ray media support is a separate future extension. Preserve upstream
case-insensitivity, 8.3 aliases, attributes, paths, timestamps, enumeration,
volumes and supported locking/sharing behavior. Internal shell files can remain
virtual rather than exposing Linux directories.

Raw host devices and host filesystem paths must be inaccessible through the DOS
interface by default. Root privileges do not provide isolation: upstream MOUNT,
IMGMOUNT, BOOT and host-integration escape routes need an explicit audit and
enforcement before production. Writable drive mappings alone are insufficient.

## Boot and hardware

Boot path: Pi firmware → ARM64 kernel → embedded initramfs → `/init` → virtual
filesystems and PI-DOS → SDL2 → DOS VM → `C:\>`.

Production suppresses routine Linux messages/cursor and has no login, launcher,
boot menu, splash daemon or intentional delay. Development retains serial logs.
Measure firmware, loading, kernel, init, storage, SDL, VM and first prompt stages
separately over repeated cold boots before assigning time/RAM targets.

| Priority | Board | CPU | Graphics |
| --- | --- | --- | --- |
| 1 | Pi 5B | Cortex-A76 | VideoCore VII / V3D |
| 2 | Pi 4B | Cortex-A72 | VideoCore VI / V3D |
| 3 | Pi 3B/B+ | Cortex-A53 | VideoCore IV / VC4 |
| 4 | Other supported 64-bit models and Compute Modules | ARMv8-A or later | Per-model VC4/V3D |

## Delivery gates

| Phase | Work | Acceptance |
| --- | --- | --- |
| 0 | Pin and build native SDL2 DOSBox-X on ARM64 | DOS prompt, DIR/CD/COPY, COM/EXE, interpreter and ARM64 dynrec work; native SDL2 confirmed |
| 1 | KMS/DRM, GLES, ALSA, USB keyboard/mouse | Direct HDMI prompt, VGA program, sound and input without a desktop; timing measured |
| 2 | Buildroot external tree, kernel, C init, image and storage | Power-on to DOS, persistent C:, root-only runtime, clean reboot/shutdown without shell/login/services |
| 3 | DOS compatibility and interpreter fallback | Representative commands/BAT, real/protected-mode, DPMI, TSR, VGA, audio and mouse software; documented limitations |
| 4 | Shared ARM64 image and multi-board validation | Targeted boards boot and run common programs; board quirks isolated; CPU/boot/RAM measured |
| 5 | Trim, optimize and prepare release | Repeatable builds, stable lifecycle/storage, source/notices and hardware regression results |

Do not begin OS-specific customization until Phase 0 integration problems are
understood. Establish a functional SDL2 engine first, direct KMS/DRM second,
and the minimal boot image third. Pi-DOS 1.0 is the self-contained Pi 5 bootable
SD image meeting Phases 0–2 with COM/EXE/BAT and persistent C:.

## Validation and risks

Add small DOS regressions as integration requires them: INT 21h, files,
directories, environments, allocation, keyboard, timers, CPU and protected mode.
Compare to pinned upstream and appropriately licensed DOS references. Maintain
a compatibility matrix with interpreter/dynrec results.

Validate text, 320×200/640×480, palettes, flipping, scrolling, aspect/VSync and
mode changes; PC speaker/OPL/PCM timing and underruns; persistence, filename
conversion, long I/O, power loss and recovery. Benchmarks record board, kernel,
page size, display, CPU core, throughput, frame time, audio and I/O latency,
boot time, steady-state and peak RAM. Avoid tests unrelated to the active gate.

The principal risks are SDL2 KMS/DRM and GLES integration, ARM64 JIT correctness,
graphics dependency size, DOS compatibility, power-loss damage and Pi hardware
differences. Mitigate with staged gates, interpreter fallback, retained upstream
behavior, measured dependency trimming, isolated persistent storage and a common
platform boundary.

## Distribution and future scope

Preserve upstream notices and prepare component license texts, corresponding
sources, changes and build instructions before distribution. Do not bundle
proprietary MS-DOS or applications without distribution rights. The integrated
DOS implementation must work independently.

Later possibilities include FreeDOS images, improved timing accuracy, more audio
devices, gamepads, USB/ISO/floppy/FAT images, CRT modes/fonts, ARM64 optimization,
other platforms, migration to a fully ported native SDL3 DOSBox-X runtime, and
experimental bare metal. All must preserve the dedicated
DOS environment as the sole user interface.
