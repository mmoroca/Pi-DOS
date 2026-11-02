# Windows 3.1 installation and CD performance

Installers can now edit writable `C:\CONFIG.SYS`; supported settings and
compatible driver commands apply on the next boot, before AUTOEXEC.BAT.
See [CONFIG.SYS support](config-sys.md).

Windows installation media must be supplied by the user. These source changes
require a rebuilt Pi-DOS image; physical CD timing and Windows mouse operation
still need confirmation on the Pi.

## Native S3 / ET4000 installer and Control Panel applet

The optional Pi-DOS Display package supplies a native 16-bit Windows installer
and a `Pi-DOS Display` Control Panel applet for Windows 3.1/3.11 and Windows for
Workgroups in 386 Enhanced Mode. It uses user-supplied vendor drivers; it does
not implement a new graphics driver. No Win32s installation is needed.

Build it with the existing Open Watcom toolchain and extracted driver folders
named `S3` and `ET4`:

```sh
make windows-video DRIVERS=/home/rtomas/Descargas/DOSBOX/DRIVERS
python3 tools/windows/check-video.py
```

Output: `output/windows-video/PIDOSVID.ZIP`. Extract the complete `PIDOSVID`
folder to the data partition, for example `C:\DRIVERS\PIDOSVID`. In Windows
Program Manager, choose File → Run and enter
`C:\DRIVERS\PIDOSVID\SETUP.EXE`. Select the matching adapter, resolution,
colour depth and small/large fonts, then Apply. Save work, exit Windows
completely and start it again. Close and reopen Control Panel to find the
new applet. The source folder is no longer required after installation.

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

Change the emulated video card in PIDOSCFG and reboot Pi-DOS before selecting
its Windows driver. The applet checks the saved machine selection when it is
available and does not alter it. Existing localized Windows font files are
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
after the first file is committed. **Windows 3.1 UI, driver startup, mode
switches, DOS sessions and physical Pi display validation remain pending.**
Use a copy of the Windows installation for the first guest validation. The
[package instructions](../src/winvideo/README.TXT) include recovery and removal.

If Apply reports `Could not create directory: C:\WINDOWS\PVIDBACK` with an
older applet, update `WINDOWS\SYSTEM\PIVIDEO.CPL` from the rebuilt package
while Windows is closed. The original directory check only accepted `EEXIST`;
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
