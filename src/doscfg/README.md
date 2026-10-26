# Pi-DOS setup

`PIDOSCFG.EXE` is an original, standalone 16-bit DOS MZ program written in C89.
It runs in real mode with 8086 instructions, uses conventional memory, and
requires neither an extender, DPMI, an x87 nor a third-party UI framework.
It is bundled as `Z:\BIN\PIDOSCFG.EXE`, on the standard Pi-DOS PATH.

The 80x25 color/monochrome interface uses BIOS keyboard services and a small text-mode
widget layer. Mouse input uses the DOS INT 33h driver when present. Keyboard
navigation works without that driver. The program restores the previous 80x25 text
screen and cursor on exit. See [the DOS manual](README.DOS) for controls and
command-line use. The image installs that manual as `C:\DOCS\PIDOSCFG.TXT`.

Six pages configure CPU, Memory, Video, Sound, Mixer and System settings. F5/F6
change pages; Tab/Shift+Tab move focus; Enter edits; F2 saves all pages; Esc
cancels. Selection dialogs support typing to filter, arrows and page keys.
**Restore defaults** (F3 or Alt+R) resets the current page only, pending Save.
The CPU default remains Pentium 133 MHz (`pentium`, `dynamic_rec`, fixed 71240),
marked `(default)` in the profile list.
Existing installations retain their configuration until explicitly changed.

## Memory, video, sound and system

RAM choices are 1, 2, 4, 8, 16, 32, 64 and 128 MiB, with 16 MiB the default.
The Memory page lists every size directly, like CPU profiles, with keyboard
and mouse selection. Help explains compatibility problems above 16 MiB. Selecting RAM clears the
additional `memsizekb` override. Saving rejects RAM beyond the chosen CPU's
address range.

The Video page uses the same selection-list control as Sound: IBM CGA,
Hercules, Tandy, IBM EGA, IBM VGA, Tseng Labs ET4000, S3 Trio64 (default),
VESA VBE 3.0 (`svga_dosbox`), and VESA VBE 2.0 (`svga_dosbox_vbe2`). Video names
are shared with the boot summary in `src/common/video_profiles.c`. The UI also
runs in Hercules monochrome text mode so users can change away from that adapter.
A separate 3dfx Voodoo checkbox enables/disables software hardware emulation.
Defaults are `voodoo_card=software`, `voodoo_maxmem=true`, `glide=false`.
Toggling the checkbox writes the full preset, using `voodoo_card=false` when
unchecked. This control does not enable Glide passthrough. Video changes need
a reboot. Existing custom video/Voodoo settings are retained until edited.

Sound presets are None, Tandy, AdLib, Creative Game Blaster/CMS, Sound Blaster
2.0, Sound Blaster Pro 2, Sound Blaster 16 (default), and Gravis Ultrasound
Classic. Each preset enables its devices and disables competing managed cards;
MIDI remains independent. PC Speaker is the first Sound control, checked when
enabled (the default). Applicable controls expose I/O address, IRQ, DMA, SB16 high DMA,
FM mode, and GUS sample RAM/patch directory. AdLib and Tandy use fixed ports.
GUS patches are not bundled; programs that require them need a populated
`C:\ULTRASND\MIDI` directory. Custom multi-card configurations are preserved
until a preset is selected. The boot table reports it as DOS sound.

### MIDI

Sound also offers independent MIDI output: None (default), Roland SC-55,
Roland GM, Roland MT-32, Roland CM-32L and External USB MIDI. The two GM
profiles use the supplied SF2 banks through FluidSynth; MT-32 and CM-32L use
Munt with separate control/PCM ROM pairs. Internal synthesis joins DOSBox's
mixer and the existing HDMI audio output. It can accompany Sound Blaster
digital effects. Select the matching music device in each game's setup.

MIDI settings opens MPU-401 interface (Intelligent by default or UART),
I/O port (330h by default) and IRQ (automatic by default). External USB MIDI
also offers a destination picker and optional SysEx delay for older modules.
Connect the interface, open the picker, and press F5 or click Rescan to refresh.
The saved identity survives ALSA client-number changes; interfaces without a
unique serial number should stay on the same USB socket. Missing or ambiguous
destinations disable MIDI for that boot rather than choosing another device.
External MIDI sends messages only: connect the module's own audio output to
speakers or a mixer. DOSBox's internal MIDI volume does not control that audio.

All selections live in `C:\SYSTEM\PIDOS.CFG`. Assets live under
`C:\SYSTEM\MIDI`: `SC55.SF2`, `GMROLAND.SF2`, and control/PCM ROMs in
`MT32` and `CM32L` subdirectories. PID 1 preserves existing asset files and
reports missing assets/devices in the DOS MIDI boot row. MIDI output or
interface changes require a reboot. Missing profile keys preserve legacy
manual MIDI configuration.

Stage the supplied assets before a future image build:

```sh
python3 tools/dos/midi-assets.py --source /home/rtomas/Datos/Emulation/bios/RePlay/bios/scummvm
```

The importer verifies `configs/midi-assets.json` hashes and stores files in
the ignored `output/midi-assets` cache. Build hooks package that cache into
the initramfs and data partition; ROMs and SF2 files are not stored in Git.

Mixer controls expose master, SB digital, FM, GUS, CMS, PC speaker, Tandy,
Tandy DAC, Disney/Covox and internal MIDI music. Values are left:right percentages
from 0 to 200; amplification above 100 can clip. Arrow keys adjust by 5%, or
choose a preset/custom stereo value. On first entering Mixer, the utility reads
the running hardware via `CONFIG -ALL -GET` and levels via `MIXER`. When selected
hardware matches, enabled channels update immediately through
`Z:\SYSTEM\MIXER.COM /NOSHOW`. The page shows LIVE or AFTER REBOOT. Cancel restores
the starting levels (at MIXER's whole-percent precision); Save keeps the preview.
Pending hardware changes suspend preview and restore the original live levels.
Unavailable live control falls back to saving for reboot. A writable `C:\TEMP`
or `C:\` is needed for an exclusively created, automatically removed query file.
Inactive channels apply when their hardware is enabled after reboot.

The System page selects keyboard layout, DOS country and an installed IANA
timezone. Searchable DOS catalogs beside the executable are generated from the
pinned runtime's country/layout definitions and packaged tzdata. There are no
large in-memory timezone arrays. Defaults remain Spanish (`sp`), Spain (`34`),
and `Europe/Madrid`. Country can also accept a custom country,codepage value;
custom timezones must exist in the installed catalog. Regional settings apply
after reboot, using the existing timezone/DOS startup handling. The Diagnostic
logs checkbox controls `[pidos] diagnostics`, disabled by default. Enabling it
captures the existing boot, runtime, network and video logs after reboot.

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

The boot table places Time directly below Date on the left. The right column
lists System, DOS CPU, DOS memory, DOS video, DOS Voodoo, DOS sound and DOS MIDI.
DOS Voodoo displays Enabled/Disabled.
A full-width Regional line below PI-DOS storage and Drives lists keyboard,
country and the active timezone. There is no cycles field. Recognized model/speed combinations show their
profile name without a prefix. Custom budgets show the
configured CPU type without an invented MHz value. `auto` displays as
`Auto (486/Pentium)`. The boot summary and utility share the profile definitions
in `src/common/cpu_profiles.c`.

## Persistence

The existing `C:\SYSTEM\PIDOS.CFG` is the only persistent configuration source.
Only edited keys are updated, including duplicates; unrelated lines are
preserved byte-for-byte. Missing edited keys are appended in their section.
Replacement lines use CRLF. Comments on separate lines are retained; trailing
text on edited assignments is replaced. Mixer percentages live under `[pidos]`
as `mixer_master`, `mixer_sb`, `mixer_fm`, `mixer_gus`, `mixer_cms`,
`mixer_speaker`, `mixer_tandy`, `mixer_tandydac`, `mixer_disney`, `mixer_midi`.
PID 1 emits separate, validated MIXER commands before the user AUTOEXEC.BAT.
This avoids overriding a user's startup commands and the DOS command-tail limit.

The file is reloaded before saving. A complete sibling `PIDOS.TMP` is written
and closed before renaming the previous configuration to `PIDOS.BAK` and moving
the new file into place. A failed replacement attempts to restore the original.
Existing temporary recovery files are never overwritten. Read errors, DOS
read-only attributes, size limits (24 KiB), invalid input and write/rename errors
are reported. Hardware and regional changes apply after reboot; mixer changes can also be
previewed immediately as described above.

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
python3 tools/image/test-midi.py
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
`bash tools/image/build.sh components` builds only the ARM64 runtime and PID 1
without assembling an SD image. The early native phase-zero helper retains
its minimal build without internal MIDI synthesis.

The Open Watcom runtime license is preserved in `LICENSES/OpenWatcom.txt`;
the image installs it with source information as `C:\DOCS\WATCOM.TXT`.
