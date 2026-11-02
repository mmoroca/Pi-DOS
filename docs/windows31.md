# Windows 3.1 installation and CD performance

Installers can now edit writable `C:\CONFIG.SYS`; supported settings and
compatible driver commands apply on the next boot, before AUTOEXEC.BAT.
See [CONFIG.SYS support](config-sys.md).

Windows installation media must be supplied by the user. These source changes
require a rebuilt Pi-DOS image; physical CD timing and Windows mouse operation
still need confirmation on the Pi.

## Unified Windows driver and Pi-DOS configuration application

Pi-DOS Configuration supplies a native 16-bit Windows installer and one
Control Panel entry for Windows 3.1/3.11 and Windows for Workgroups in 386
Enhanced Mode. Its main window opens Display, Audio/MIDI, and Pi-DOS settings.
No Win32s is needed. The applet is named `PIDOS.CPL`. Setup automatically
removes the previous `PIVIDEO.CPL`, leaving one Control Panel entry.

Build with Open Watcom and extracted video folders named `S3` and `ET4`:

```sh
python3 tools/windows/package-audio.py --fetch
make windows-drivers DRIVERS=/home/rtomas/Descargas/DOSBOX/DRIVERS
python3 tools/windows/check-video.py
python3 tools/dos/test-config.py
```

Output: `output/windows-video/PIDOSDRV.ZIP`. `PIDOSVID.ZIP` is a compatibility
copy and `make windows-video` remains an alias. Extract the full `PIDOSDRV`
folder to `C:\DRIVERS\PIDOSDRV`, keeping `SETUP.EXE`, `PIDINST.DLL` and
`PIDOS.CPL` together. Run `C:\DRIVERS\PIDOSDRV\SETUP.EXE` inside Windows and
choose **Install / Reinstall**. The opening prompt reads: "Install Pi-DOS
Windows drivers and configuration controls in Control Panel?"

Setup replaces a previous `PIDOS.CPL` and removes `PIVIDEO.CPL` automatically.
If an applet is loaded, close Control Panel and Pi-DOS Configuration, then
click Retry. Reopen Control Panel after installation. The installer runs
under the separate module name `PIDINST`, so Windows cannot return the older
cached applet when Setup loads its installation code. Manual replacement
from DOS is no longer required for an ordinary upgrade.

Setup also offers **Uninstall** (`SETUP.EXE /UNINSTALL` on the command line).
It removes both applet names while retaining installed vendor drivers,
audio caches, Windows INIs, Pi-DOS settings and backups. Install / Reinstall
can then add the controls again. Applet replacement uses a staged `PIDOS.NEW`
and temporary `PIDOS.OLD` / `PIVIDEO.OLD` backups with rollback. Interrupted
recovery files are retained and block another replacement until recovered.
Host checks exercise fresh install, legacy migration, reinstall, uninstall
and rollback after failures while moving the previous applets or installing
the replacement. Actual Windows guest upgrade/uninstall still needs checking.

### Shared Pi-DOS settings

The native settings page reuses `src/doscfg/config.c` and the DOS form
descriptions, covering CPU, Memory, Video, Sound, Mixer, Input, Network and
System. Select a setting, Edit, then Save. The shared save engine preserves
comments and unrelated edits, validates resources and keeps
`C:\SYSTEM\PIDOS.BAK`. These changes require a Pi-DOS reboot. Mixer settings
save boot levels without a live volume preview. External USB MIDI discovery
remains in DOS PIDOSCFG; its saved destination identity can be retained/edited
in the Windows MIDI settings page. Keyboard/country/timezone catalogs come
from `Z:\BIN`.

### Existing Windows audio drivers

The shared Audio and MIDI page selects and installs existing Windows 3.1
sound drivers. Public archives, hashes, vendor card mappings and source
references are recorded in `configs/windows-audio.json`. No new audio driver
is implemented. Original packages establish Windows support; the new
Pi-DOS installation paths and playback still require guest validation.

| Pi-DOS device | Windows installation from the shared application |
| --- | --- |
| Sound Blaster 2.0 | Native install of Creative SBW31 / SB20SND + SBFM |
| Sound Blaster Pro 2 | Native install of Creative SBP2W31 / SBPSND + SBP2FM + SBPAUX |
| Sound Blaster 16 | Native install of Creative SB16WU / SB16SND + SB16FM + SB16AUX |
| Gravis UltraSound Classic / GF1 | Launch original Gravis 4.11 installer after closing Windows |
| AdLib | Open Windows Drivers / Add / Ad Lib; Windows installation disks may be requested |
| PC speaker | Native install of Microsoft's SPEAK / SPEAKER.DRV (wave playback) |
| Game Blaster / CMS, Tandy | Explicitly unavailable: no confirmed compatible Windows 3.x package located |

[Microsoft's card/driver table](https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/124/944.HTM)
identifies SBW31 for SB2 and SBP2W31 for the CT1600 Pro 2. Each Creative
package also includes its original OEMSETUP.INF and installation README.
[Gravis' archived support page](https://www.infania.net/misc/gona/GRAVIS/index.html)
identifies 4.11 as GF1 Windows 3.1/3.11 software, including Windows driver
5.51; [DOSBox Staging's guide](https://github.com/dosbox-staging/dosbox-staging/wiki/Windows-3.1x#gravis-ultrasound)
documents installing it under emulation. The PnP/InterWave package is not used.
Microsoft documents the PC speaker package in
[KB138857](https://www.betaarchive.com/wiki/index.php/Microsoft_KB_Archive/138857).

Setup caches all packages below `WINDOWS\PIDOSAUD`, so the extracted source
folder can subsequently be removed. SB16WU uses the `SB16WU` cache folder,
leaving an older full SB16 installer cache intact. In Pi-DOS settings choose
the emulated card, save and reboot Pi-DOS first. Reopen Audio and MIDI, choose
the corresponding Windows driver and click **Install selected driver**.
The Creative and Gravis paths check the running BLASTER/ULTRASND resources
against saved settings. SB Pro 2 uses BLASTER type 5, SB2 type 3 and SB16 type 6.

Native installation copies the selected vendor drivers and supporting DLLs
into Windows SYSTEM, applies the vendor-documented driver aliases and
I/O/IRQ/DMA resource keys to staged INIs, and commits using the existing
rollback transaction. SB Pro's mixer is also copied. Existing different
binaries are never overwritten; use Windows Drivers / Add / Unlisted with
the cached OEM disk for a vendor upgrade. VSBPD.386 is referenced in its
card-specific cache folder. The installer replaces only legacy sound VxD
entries and preserves repeated unrelated `device=` entries. Existing MIDI
maps are preserved. Restart Windows after installation, then select or
create the desired FM or external MIDI mapping using Windows MIDI Mapper.
Windows' common Timer, MIDI Mapper, MCI Sound and MCI MIDI Sequencer files
must already be installed; the Windows Drivers button provides their normal
installation UI and can request the user's Windows disks.

For Gravis, save work before continuing: `ExitWindowsExec` closes Windows,
runs the original DOS installer and restarts Windows when it returns. Install
into the displayed `ultradir`, use Custom Setup to update the displayed Windows
directory, and use the shown I/O/IRQ/DMA settings. Do not install SBOS or
Mega-Em emulation. After restart, choose the appropriate Ultra MIDI map
(e.g. Ultra 1024K for the default 1 MB card). The DOS helper preserves root
`AUTOEXEC.BAT` and `CONFIG.SYS`, since Pi-DOS already initializes DOS hardware.
Windows and installed Gravis patch files are retained. Interrupted recovery
state is kept and blocks another run.

Focused checks cover package integrity, executable format, sound VxD
preservation and staged INI rollback. The actual DOS helper was also exercised
under the packaged ARM64 runtime using a harmless substitute installer:
`python3 tools/windows/check-audio-run.py --sdl-library-path PATH`.
This checks startup-file recovery, not the real Gravis installer or playback.

### Windows display modes

The existing Display page retains its mode and font controls. Apply, save
work, exit Windows completely and start it again to use the selected mode.

The build reads `dac-crt.bin`, validates its checksums and intersects its
advertised sizes with the vendor OEM mode lists. The current intersection
is **640×480, 800×600 and 1024×768**, giving 34 combinations including font
sizes. It excludes 1152×864, 1280×1024 and 1600×1200. EDID sizes absent from
the vendor drivers, such as 832×624 and the HD sizes, are also unavailable.
The compiled catalogue supplies every choice; no arbitrary resolution entry
is accepted. Rebuild after changing the EDID or vendor package.

S3 offers 256, 32K and 64K colours at all three sizes, plus 16.7M colours
(32-bit pixels) at 640×480 and 800×600. ET4000 offers 16, 256, 32K and 64K
colours at 640×480 and 800×600, and 16/256 colours at 1024×768. Its 24-bit
TrueColor entry is excluded because the emulator's ET4000 DAC implements
15/16-bit high colour. High colour requires Sierra RAMDAC emulation (the
runtime's default); video memory must also be sufficient for the chosen mode.
These are driver mode choices, not guarantees of physical monitor timing.
Host refresh selection and CRT/LCD configuration remain Pi-DOS settings.

Change the emulated video card in Pi-DOS settings or DOS PIDOSCFG and reboot
Pi-DOS before selecting its Windows driver. The Display page checks the saved
machine selection when it is available and does not alter it. Existing localized Windows font files are
retained; missing fonts come from the supplied English packages. DBCS Windows
is outside this package's scope.

Before the first change, `WINDOWS\PVIDBACK` receives the original INI files.
Before every change, `WINDOWS\PVIDLAST` receives the previous files. The
applet prepares the new INIs separately and uses the shared C rename
transaction to install them, rolling back a failed commit. `Restore previous`
restores the last full INI snapshot, including unrelated settings at that
time. If Windows cannot start, run `PVRESTOR.BAT` from the Windows directory
at the DOS prompt, then run `WIN`. There is no automatic timed rollback.

The installer preflights all payloads and refuses to overwrite different
existing driver binaries. It retains existing fonts and vendor INIs. Files
are expanded by their SZDD signatures, including ET4 files which retain
`.DRV`/`.FON` extensions despite being compressed. Vendor binaries are used
only in the generated local package and are not added to the repository.
The build manifest records source/payload hashes and the EDID hash.

Host checks cover the catalogue, decompression, NE executable types and
exports, and the actual C configuration transaction including a forced failure
after the first file is committed. The prior display applet and directory fix
were user-confirmed working. **The shared Windows UI and real audio installation
and playback still need Pi validation.**
Use a copy of the Windows installation for the first guest validation. The
[package instructions](../src/winvideo/README.TXT) include recovery and removal.

If Apply reports `Could not create directory: C:\WINDOWS\PVIDBACK` with an
older applet, run the rebuilt `SETUP.EXE` and choose Install / Reinstall.
Setup migrates `PIVIDEO.CPL` to `PIDOS.CPL`. The original directory check only accepted `EEXIST`;
DOS can instead return error 5 (mapped to `EACCES`) for an existing directory.
The updated check verifies directory attributes, rejects a conflicting regular
file and includes DOS/runtime error codes on a genuine creation failure.
Existing backups and vendor drivers can be retained.

## Black screen when formatting a floppy in File Manager

The diagnostic log confirmed `E_Exit: Unhandled Dos 21 call 53`. Windows 3.11
File Manager requests INT 21h AH=53h after writing the boot sector. The runtime
previously terminated at that unimplemented service, closing the display and
leaving Pi-DOS without a running emulator.

Patch `0043-dos-bpb-to-dpb.patch` implements the FAT12/FAT16 BIOS parameter
block to DOS drive parameter block conversion. It supports the DOS 3.x and
DOS 4+ layouts, preserves caller-owned linkage/device fields, and rejects
invalid or unsupported layouts with DOS error 13 instead of exiting. The
conversion only updates the caller's memory; it does not access the disk.

The focused check is `python3 tools/image/test-bpb-to-dpb.py`. Actual formatting
in Windows with a physical USB floppy still needs Pi validation. An aborted
format may already have changed the floppy's boot sector; format that disk
again after updating. To update an existing SD card, replace BOOT's `Image`
with the rebuilt `output/buildroot-arm64/images/Image` while the Pi is off.
The Windows installation on the PI-DOS data partition can be kept.

## Slow installation scan and file reads

Windows Setup searches for an existing installation. Pi-DOS previously sent
physical media-status commands for every directory-search step, even when the
ISO directory was already cached. Successful checks are now reused for up to
100 ms. The search still runs, but repeated entries no longer each require a
USB round trip.

Cooked data reads now use a 64 KiB read-ahead buffer instead of fetching every
2 KiB sector separately. Prefetch stops at track boundaries. If a speculative
read fails, the backend retries only the requested sectors, provided the medium
has not changed. Raw sectors and CD audio bypass this buffer.

Media changes, ejection and disconnect detection clear the buffer. Cached
directory/data access can take up to 100 ms to notice a physical swap; cache
misses and raw/audio reads check the drive immediately. Existing DOS handles
are rejected once the change is detected.

## Mouse stops when Windows starts

The DOS installer uses the built-in INT 33h interface. Windows uses its own
mouse driver, which needs relative motion with the mouse captured. The SDL2
fullscreen surface code previously toggled capture on video-mode changes,
which could release it as Windows loaded. It now explicitly enables capture
for CRT and LCD modes. Startup also enables automatic capture.

On an existing image, try Ctrl+F10 to capture the mouse again. If it still does
not move, exit Windows with the keyboard, run `SETUP` from its installed
directory and check that the mouse is set to Microsoft or IBM PS/2. See the
[DOSBox-X Windows 3.1 guide](https://dosbox-x.com/wiki/Guide%3AInstalling-Windows-3.1x)
for driver choices.

## PATH exceeds 127 characters

The previous system PATH was 128 characters after networking directories were
appended, before Windows added its own directory. It is now 94 characters,
or 105 with `;C:\WINDOWS`, leaving space below Setup's 127-character limit.

`Z:\DEBUG`, `Z:\PATCHING` and `Z:\4DOS` remain available through their full
paths. They are no longer on the default search path.

For an existing image, this prompt command provides the shorter path:

```dos
SET PATH=C:\PROGRAMS\TOOLS;C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\NETWORK;Z:\NETWORK\MTCP;C:\WINDOWS
```

Use your actual Windows directory if different. After installation, ensure
`C:\AUTOEXEC.BAT` includes `SET PATH=%PATH%;C:\WINDOWS` once, since Setup may
have omitted it after the warning. Review existing custom PATH commands before
making this persistent; extra directories still count toward Setup's limit.
