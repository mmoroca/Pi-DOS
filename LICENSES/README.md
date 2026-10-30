# Source provenance

No project-wide license has been selected for original Pi-DOS work.

The Phase 0 workflow downloads unmodified upstream archives pinned in
`sources.lock.json` and keeps their existing copyright and license files:

- [DOSBox-X](https://github.com/joncampbell123/dosbox-x/tree/4f19017c5f565dc40d01fede0f1382892e7243d7)
- [SDL2 2.32.10](https://github.com/libsdl-org/SDL/tree/5d249570393f7a37e037abf22cd6012a4cc56a71)

These local development builds are not a distribution manifest. Before shipping
images, inventory all bundled code, libraries, drivers and firmware, preserve
their notices, and prepare required sources and build instructions. The release
gate remains open until that work is complete.

The build applies the separately maintained `patches/dosbox-x/` series to the
verified upstream archive. Include those changes with the corresponding
component source and notices when preparing a distribution.

The Buildroot image uses the pinned upstream Buildroot release and its package
hashes, plus the external recipes under `br2-external/`. Original `pi-dos-init`
code is marked `unknown` in Buildroot metadata until a project license is
selected. The generated development image is not a completed release license
or corresponding-source bundle.

The shared ARM64 image also includes Raspberry Pi boot firmware from the
`rpi-firmware` package, pinned by the external tree to
`c3c104bcfd90a5e6f7c89f00204ac11f1e9ffa6f` (22 May 2026). This replaces the
December 2025 snapshot with a confirmed fix for Pi 4 VL805 USB timeouts.
Its archive and BSD-3-Clause notice are hash-checked using
`br2-external/board/raspberrypi/rpi-firmware.hash`; the image
retains `LICENCE.broadcom` on BOOT beside the firmware files.

Doszip Commander DOS 2.68 is vendored under
`br2-external/board/raspberrypi/dos/doszip/` using its GNU GPL v2-or-later license.
The image includes the upstream license notice, the GPL v2 text, and the matching
unmodified source archive (`SOURCE.ZIP`) in `C:\PROGRAMS\DOSZIP`. Archive URLs
and SHA-256 hashes are recorded in the adjacent `SOURCE.TXT` and
[DOS payload documentation](../br2-external/board/raspberrypi/dos/README.md).

`PIDOSCFG.EXE` is original Pi-DOS code compiled with the unmodified Open Watcom
`2026-10-01-Build` toolchain. The compiler/runtime archive is pinned with its
SHA-256 in `tools/dos/toolchain.json`. Its runtime license is retained as
`LICENSES/OpenWatcom.txt` and installed with the upstream source URL as
`C:\DOCS\WATCOM.TXT`. Utility sources and build instructions are in `src/doscfg/`.

The networking image adds Buildroot's hash-pinned SDL2_net (Zlib), libslirp
(BSD-3-Clause) and libpcap packages. mTCP 2025-01-10 is installed on
`Z:\NETWORK\MTCP` from the checksum-pinned FreeDOS package recorded in
`configs/network-assets.json`. Its GPL v3 notice and corresponding source are
included beside the binaries as `COPYING.TXT` and `SOURCE.ZIP`; `PACKAGE.TXT`
retains package provenance. Windows for Workgroups, Microsoft TCP/IP-32 and
Novell ODI/IPX drivers are user-supplied and are not included in the image.

The local development image installs a selected benchmark pack under
`C:\SYSTEM\BENCH`. Its upstream compilation is Phil's DOS Benchmark Pack v1.6;
archive URLs and SHA-256 hashes are pinned in `configs/benchmark-assets.json`.
Superscape 3DBench 1.0c, Chris Dial's benchmark and PC Player executables/data
remain unmodified, with available embedded notices retained. The upstream
compilation README is preserved as `PHIL.TXT`. It does not establish a blanket
redistribution license for those components; their public-distribution review
remains part of the existing release gate.

Doom 1.9 and Quake 1.06 are original shareware releases, not GPL game data.
The complete original installers are retained per game as `ORIG.ZIP`, alongside
all original installed documentation, including Quake's `SLICNSE.TXT` and
`LICINFO.TXT`. Their executables and game data match Phil's pack byte for byte.
Phil's benchmark configurations are added separately. Thomas Perl's DOOMFPS
helpers retain their supplied source files and author notices; public-release
permission for the helpers also remains to be established. The generated
`SOURCE.TXT` records provenance, and `FILES.JSN` inventories the installed files.
Preparing this local image does not close the project's distribution gate.

The demoscene pack under `C:\DEMOS` uses the original scene.org releases pinned
in `configs/demo-assets.json`. The executables/data are unchanged; every archive
and its included notices are retained per production under `ORIG\DISK1.ZIP`
(plus `DISK2.ZIP` for Second Reality). The extracted multi-disk installation
keeps the first shared filename; both originals remain in the ZIPs. Pi-DOS adds
only its menu, guide, inventory and source record. Copyright remains with Future
Crew, Triton, NoooN, Plant/EMF, CNCD and the respective contributors. Online
availability does not establish permission to distribute every production in a
public SD image. Review each original notice under the existing release gate.
