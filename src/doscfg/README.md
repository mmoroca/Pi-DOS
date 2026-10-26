# Pi-DOS CPU setup

`PIDOSCFG.EXE` is an original, standalone 16-bit DOS MZ program written in C89.
It runs in real mode with 8086 instructions, uses conventional memory, and
requires neither an extender, DPMI, an x87 nor a third-party UI framework.
It is bundled as `Z:\BIN\PIDOSCFG.EXE`, on the standard Pi-DOS PATH.

The 80x25 color interface uses BIOS keyboard services and a small text-mode
widget layer. Mouse input uses the DOS INT 33h driver when present. Keyboard
navigation works without that driver. The program restores the previous 80x25 text
screen and cursor on exit. See [the DOS manual](README.DOS) for controls and
command-line use. The image installs that manual as `C:\DOCS\PIDOSCFG.TXT`.

The **Restore defaults** button (F3 or Alt+R) selects Pentium 133 MHz; it does not write
until Save is activated. Cancel also cancels a pending restoration. The default
profile sets `cputype=pentium`, `core=dynamic_rec`, and `cycles=fixed 71240`.
Fresh images use this profile. Existing user configuration files are preserved;
use Restore defaults and Save to switch an existing installation.

## CPU profiles

| ID | Label | cputype | core | cycles |
| --- | --- | --- | --- | --- |
| auto | Automatic | auto | dynamic_rec | auto |
| 286-12 | 286 12 MHz | 286 | normal | fixed 1510 |
| 386-33 | 386DX 33 MHz | 386 | dynamic_rec | fixed 6075 |
| 486-33 | 486DX 33 MHz | 486 | dynamic_rec | fixed 12010 |
| 486-66 | 486DX2 66 MHz | 486 | dynamic_rec | fixed 23880 |
| 486-100 | 486DX4 100 MHz | 486 | dynamic_rec | fixed 33445 |
| pentium-100 | Pentium 100 MHz | pentium | dynamic_rec | fixed 53430 |
| pentium-133 | Pentium 133 MHz (default) | pentium | dynamic_rec | fixed 71240 |
| mmx-200 | Pentium MMX 200 MHz | pentium_mmx | dynamic_rec | fixed 114657 |

MHz values are approximate speed equivalents, using the pinned DOSBox-X speed
presets. Unlike those upstream speed-only presets, these Pi-DOS profiles also
select the CPU model and execution core. They do not change memory or FPU options.
These profiles do not establish application compatibility
or that every supported Pi can sustain the requested budget.

The boot table places Time below Date on the left, with DOS CPU directly above DOS video on the right
and no cycles field. Recognized model/speed combinations show their
profile name without a prefix. Custom budgets show the
configured CPU type without an invented MHz value. `auto` displays as
`Auto (486/Pentium)`. The boot summary and utility share the profile definitions
in `src/common/cpu_profiles.c`.

## Persistence

The existing `C:\SYSTEM\PIDOS.CFG` is the only persistent configuration source.
The utility updates the three CPU keys, including duplicates, preserves other
lines byte-for-byte, and appends missing keys in a CPU section. Replacement CPU
lines use CRLF. Comments on separate lines are retained; old CPU assignment
values, including any trailing text on those assignments, are replaced.

The file is reloaded before saving. A complete sibling `PIDOS.TMP` is written
and closed before renaming the previous configuration to `PIDOS.BAK` and moving
the new file into place. A failed replacement attempts to restore the original.
Existing temporary recovery files are never overwritten. Read errors, DOS
read-only attributes, size limits (24 KiB), invalid input and write/rename errors
are reported. Configuration changes apply after reboot, using existing startup
handling; no live emulator or Linux settings API is added.

When the current three settings do not match a profile, the interface shows
Custom settings with no profile preselected. Cancel never saves. Missing keys
inherit the packaged defaults. Manual advanced CPU settings can still be edited
with `EDIT`.

## Rebuilding

The compiled utility is tracked so ordinary image builds need no DOS compiler
and no additional network downloads. To rebuild it on Linux x86-64:

```sh
python3 tools/dos/bootstrap.py
make dos-config
python3 tools/dos/test-config.py
python3 tools/image/test-startup.py
make image
```

Bootstrap verifies the SHA-256 of the official Open Watcom
`2026-10-01-Build` archive, pinned in `tools/dos/toolchain.json`, and extracts it
to `output/tools/openwatcom`. The archive is cached under `dl/`. It makes no
system-wide installation. An existing compiler installation can be selected
with `WATCOM=/path/to/watcom make dos-config` (with Linux x64 `binl64` tools).

The build uses `-0 -ms -os` (8086, small model, size optimization) and treats
warnings as errors. The adjacent `PIDOSCFG.build.json` records source hashes,
compiler driver hash and executable hash. Image verification rejects stale
sources or a stale executable and checks the packed initramfs copy.

The Open Watcom runtime license is preserved in `LICENSES/OpenWatcom.txt`;
the image installs it with source information as `C:\DOCS\WATCOM.TXT`.
