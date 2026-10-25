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
