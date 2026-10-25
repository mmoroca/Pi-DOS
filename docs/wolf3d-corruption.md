# Wolfenstein 3D: incorrect x87 stack updates

Investigated on 26 October 2026 using the latest Pi-DOS image and the user's
game directory. The defect is in overlapping FPU status-register views under
compiler optimization. It affects the portable x87 implementation shared by
the normal interpreter and ARM64 dynrec.

## Baseline and reproduction

| Input | Verified value |
| --- | --- |
| DOSBox-X source | `4f19017c5f565dc40d01fede0f1382892e7243d7`, release 2026.08.31 |
| Pi-DOS executable | AArch64 ELF, `C_TARGETCPU=ARMV8LE`, `C_DYNREC=1`, `C_FPU=1`, `C_FPU_X86` disabled |
| Compiler | Buildroot/Bootlin GCC 14.3.0, `-O2`, strict aliasing enabled |
| Production SDL | Native SDL2 2.32.10, KMS/DRM; no SDL3 |
| Image default CPU core | `dynamic_rec`; persistent `C:\SYSTEM\DOSBOX.CFG` can override it |
| Working reference | Installed amd64 Snap, DOSBox-X 2026.08.31, SDL2 |
| Game | `WOLF3D.EXE`, 98,402 bytes, LZEXE 0.91 compressed, `.WL6` data |
| EXE SHA-256 | `0514e93d1c330b5acadd2865da84da1ad937056c5c8daef34a63726deb8e2a09` |

The provided game files were copied into isolated run directories. Their
originals were not modified or added to the repository. A manifest is in
`output/investigation/wolf3d/game.sha256`.

The actual ARM runtime was run through QEMU user mode. A separate SDL2 2.32.10
build enabled the dummy video/audio drivers missing from the production
library. The production SDL library and generated image were not used for
headless presentation. The Snap reference ran on an isolated Xvfb display.
These runs test emulation behavior, not physical Pi display/cache behavior.

Internal AVI captures reproduced the single correct center strip with both
`core=normal` and `core=dynamic_rec`, `cputype=386`, and `machine=vgaonly`.
The Snap rendered the same starting room correctly. Thus host presentation
is not required to trigger this failure. The interpreter run also excludes
a dynrec-only native instruction-cache defect as its cause.

Use the game's own direct-level option to avoid menu-input timing:

```dos
DX-CAPTURE /V /-D WOLF3D.EXE -tedlevel 0 -nowait
```

`DX-CAPTURE` is DOSBox-X's built-in command. Configure a writable host
`[dosbox] captures` directory first. Quit the game normally to close the AVI.
For an isolated host invocation, also use `-defaultconf -defaultmapper
-nopromptfolder -defaultdir <run-directory>`, explicitly select the core,
and mount a copy of the game. QEMU is slower than native execution; allow
the loading screen to finish before judging the result.

## Cause and fix

`include/fpu.h` defines `RegBit<T, bitno, nbits>` specializations as overlapping
views of the same 16-bit control/status register. The unannotated types let
GCC cache one view's register value across a write through a different view.

The portable `FPU_FPTAN()` computes the tangent, pushes `1.0`, then clears C2.
With this build, the final C2 write restores the old TOP bits. The pushed
value exists in the register array but the guest sees the old stack top.
`FPU_FSINCOS()` has the same failure. The before/after regression executes
the actual helpers from `src/fpu/fpu_instructions.h`:

```text
Before: FPTAN   TOP=7 SW=7900 ST0=0.25534192122103627 ST1=0
Before: FSINCOS TOP=7 SW=7900 ST0=0.24740395925452294 ST1=0
After:  FPTAN   TOP=6 SW=7100 ST0=1 ST1=0.25534192122103627
After:  FSINCOS TOP=6 SW=7100 ST0=0.96891242171064473 ST1=0.24740395925452294
```

The test starts with TOP=7 and C0/C2/C3 set. Both instructions must push,
clear C2 and preserve C0/C3. A separate DOS instruction probe also confirmed
the incorrect stack through the full CPU interpreter before the patch.

Patch [0007](../patches/dosbox-x/0007-preserve-overlapping-fpu-register-bits.patch)
adds `GCC_ATTRIBUTE(may_alias)` to the register-view type. This makes writes
through different specializations invalidate each other's cached values.
The instruction order, CPU defaults and VGA implementation are unchanged.

After rebuilding, the full DOS instruction probe produced the correct tangent
and stack values. Internal captures of the user's game then showed correct
walls, door, floor and weapon with both ARM cores, matching the Snap reference.
Captures, commands and logs are under `output/investigation/wolf3d/`:

| Run | Result |
| --- | --- |
| `snap-direct-normal` | Correct starting room |
| `arm-direct-dynamic_rec` (before patch) | Corrupted viewport, correct center strip/HUD |
| `arm-confirm-normal` (before patch) | Same corruption |
| `arm-fixed-dynamic_rec` | Correct starting room |
| `arm-fixed-normal` | Correct starting room |

The `latest.png` files show the selected frames. A physical Pi rerun remains
necessary to confirm the updated image on hardware.

The SD image was regenerated at
`output/buildroot-arm64/images/pi-dos-arm64-sdcard.img`. The existing image
validator passed. The runtime extracted from `rootfs.cpio` matches the installed
runtime, and its executable `.text` matches the unstripped build used for the
successful captures. Hashes are recorded in
`output/investigation/wolf3d/packaging-verification.json`.

This explains the viewport/HUD split: Wolfenstein initializes trigonometric
tables before its integer raycaster uses them. Its DOS maths library uses
`FPTAN` in those calculations. A guest-memory dump located the raycaster's
tangent table through its segment-load instruction; the table contained
reciprocals where small tangent values belonged. For example, entry 6 was
5,776,946 rather than approximately 743. The zero-input maths path is
handled separately in the game library, consistent with the surviving
center strip. The HUD does not use those raycasting tables.

Relevant upstream sources:

- [FPU register views](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/include/fpu.h)
- [Portable FPU helpers](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/src/fpu/fpu_instructions.h)
- [Wolfenstein table initialization](https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/WL_MAIN.C)
- [Wolfenstein integer raycaster](https://github.com/id-Software/wolf3d/blob/master/WOLFSRC/WL_DR_A.ASM)

The released Wolfenstein source is a guide to the renderer; it is not a
symbol map for the user's earlier commercial executable. Zone Raiders
issue #6703 / PR #6667 is not evidence for this specific fix.

## Focused regression

[tests/cpu/fpu-registers.cpp](../tests/cpu/fpu-registers.cpp) includes the actual
portable helpers and checks FPTAN/FSINCOS stack values and status bits. From
the project root, with the configured DOSBox-X source available:

```sh
runtime_dir=output/buildroot-arm64/build/pi-dos-runtime-4f19017c5f565dc40d01fede0f1382892e7243d7
mkdir -p output/investigation/wolf3d
output/buildroot-arm64/host/bin/aarch64-linux-g++ -std=gnu++14 -O2 \
  -I"$runtime_dir" -I"$runtime_dir/include" -I"$runtime_dir/src" \
  tests/cpu/fpu-registers.cpp -o output/investigation/wolf3d/fpu-registers
qemu-aarch64 -L output/buildroot-arm64/target \
  output/investigation/wolf3d/fpu-registers
```

Alternatively run the resulting executable natively on an AArch64 Linux
host with the matching runtime libraries. It exits 0 only when both checks
pass. The unpatched header fails both checks with GCC 14.3 at `-O2`.

Buildroot disables dependency tracking, so changing `fpu.h` alone does not
recompile existing objects. For a clean package rebuild after adding the
patch, use Buildroot's `pi-dos-runtime-dirclean`, then `make image`. New
source extractions and the native build helper apply the patch series.
