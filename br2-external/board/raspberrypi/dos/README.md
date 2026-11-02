# Personalized DOS editor

`EDIT.COM` is the user's personalized copy of the PEDIT Light 4.00 editor
bundled with the pinned DOSBox-X source. Imported from
`/home/rtomas/Descargas/EDIT.COM` on 2026-10-23.

SHA-256: `31e07b70ac6db2959a975c3cb10dde488de0cbb2908e4775449a93e00c63216a`.

It has the same 97,619-byte size as the upstream embedded `EDIT.COM`; only
five bytes differ, at offsets `0x154f2`, `0x154f3`, `0x154f4`, `0x154fe` and
`0x15509`, within its settings block. Preserve the upstream component notices.

`post-image.sh` installs this seed as `C:\PROGRAMS\TOOLS\EDIT.COM` on PI-DOS.
The existing DOS PATH selects it before the read-only editor on Z:.
Preferences saved by PEDIT modify this writable executable directly.
On existing writable installations, startup moves `C:\SYSTEM\EDIT.COM` to this
location, preserving saved preferences. An existing destination is kept.

## Pi-DOS CPU setup

See also [Pi-DOS CPU setup source and build instructions](../../../../src/doscfg/README.md).
`PIDOSCFG.EXE` is installed in `Z:\BIN` and available by typing `PIDOSCFG`.
It offers keyboard and optional mouse CPU profile setup, edits `C:\SYSTEM\PIDOS.CFG`,
and backs up the previous file as `PIDOS.BAK`. Run `REBOOT` after saving.
Its manual and the Open Watcom runtime notice are included in `C:\DOCS`.

## User startup template

`AUTOEXEC.BAT` contains comments for user startup commands, without duplicating
the system's environment defaults. The build packages a CRLF copy as
`/pi-dos/AUTOEXEC.BAT`. Fresh images and writable boots create `C:\AUTOEXEC.BAT`
only when missing, preserving existing files and customizations.

`CONFIG.SYS` is also seeded as a writable file on C: only when missing. PID 1
imports supported DOS directives on each boot, and the runtime registers C:
before processing configuration commands. See [CONFIG.SYS support](../../../../docs/config-sys.md)
for supported settings, driver loading and limitations.

## ARJ

`ARJ.EXE` is the user's MS-DOS ARJ 2.41 binary, copied from
`/home/rtomas/Descargas/arj.exe`. It identifies itself as shareware,
copyright 1990-93 Robert K Jung. Its original notices are preserved.

SHA-256: `bf37548e55ea0fed2520e5f91449b2566292b2437165dc27db8e599e2d539010`.

The post-build hook installs it in `/pi-dos/drivez/BIN/ARJ.EXE`. DOSBox-X's
existing drivez extension exposes it as `Z:\BIN\ARJ.EXE`; the original Z: layout
and built-in utilities are retained.

## Doszip Commander

`doszip/` contains the unmodified DOS 2.68 release and matching source archive
from the upstream project's DOS directory:

- Binary: https://downloads.sourceforge.net/project/doszip/DOS/dzbin268.zip
  SHA-256: `5ddd77b69194fe115c9361cd1f6ec31065d561ab7010858677468efb32942096`.
- Source: https://downloads.sourceforge.net/project/doszip/DOS/dzsrc268.zip
  SHA-256: `96df1ef800776e1d3764eabee5ec56120e250abe20cf4209af2c71d1b76b82db`.

`DZ.EXE` is the DOS loader and requires the adjacent `DZ.DOS` main program.
`DZ.TXT` is upstream's help file. `LICENSE.TXT` retains the original notice;
`COPYING.TXT` supplies the GNU GPL v2 text. The program permits GPL v2 or later.
`SOURCE.ZIP` is the unchanged source archive; `SOURCE.TXT` records provenance.

The image installs these files in writable `C:\PROGRAMS\DOSZIP` and supplies
`C:\SYSTEM\DZ.BAT` on the default PATH. The launcher passes the configuration
directory explicitly so preferences/history remain with the application.

## Benchmark pack

`bench/` contains the original Pi-DOS `BENCH.BAT` launcher, `MENU.BAT` menu
and DOS manual. `tools/dos/benchmark-assets.py` fetches checksum-pinned archives
from `configs/benchmark-assets.json`; `post-image.sh` installs the selected
benchmarks in writable `C:\SYSTEM\BENCH`, with `C:\SYSTEM\BENCH.BAT` already
on the default PATH. No emulator settings are changed by the menu.

The selection is 3DBench 1.0c, Chris's benchmark (VGA/SVGA), PC Player
(VGA/SVGA), Doom (minimum/maximum detail and FPS conversion), and Quake
(320x200, 360x480, 640x480). Full original Doom/Quake shareware payloads and
installers preserve documentation omitted by the upstream compilation.
Game executables/data must match that compilation byte for byte at packaging.
Archive extraction uses `bsdtar`, never the DOS self-extractors. DOS files use
8.3 paths, and the image verifier checks the complete staged file inventory
against the actual SD image. The build supports the existing offline cache.
Quake launch arguments retain lowercase spelling: its PAK demo names and
startup switches are case-sensitive. Image verification checks the selected
demo names against the PAK directory and requires the sound-disabling flags.

## Demoscene pack

`demos/` supplies the `DEMOS.BAT` launcher and the base DOS guide.
`tools/dos/demo-assets.py` fetches the original scene.org releases pinned in
`configs/demo-assets.json`, generates a numbered text menu, and stages them in
`C:\DEMOS`. `C:\SYSTEM\DEMOS.BAT` is on the existing PATH. Select 1-9, H for
the guide, or Q to exit. Inside runs its own sound setup before playback.

The selection is Second Reality, Unreal, Panic, Crystal Dream, Crystal Dream II,
Stars: Wonders of the World, Caero, Inside and the Fishtro intro. Second Reality
installs both disks; Stars uses the archive's corrected 2014 DOS executable;
Caero uses its final release. All extracted payloads remain unmodified, and each
production retains complete original downloads under `ORIG\DISK1.ZIP` (and
`DISK2.ZIP` for Second Reality). `SOURCE.TXT` records provenance and `FILES.JSN`
inventories the pack for image verification. Physical Pi playback is pending.

`make demos` creates a ready-to-copy `output/pi-dos-demos.zip` for existing
cards. No new DOS helper executable or changes to emulator settings are needed;
the menu uses the existing `Z:\DOS\CHOICE.COM`. Cached releases work offline.

See [source provenance](../../../../LICENSES/README.md) for component notices
and the still-open public-release license review.
