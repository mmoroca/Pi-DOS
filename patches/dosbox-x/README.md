# DOSBox-X patch series

Base: `4f19017c5f565dc40d01fede0f1382892e7243d7` (`dosbox-x-v2026.08.31`).

`0001-reject-invalid-display-dimensions.patch` fixes unknown desktop dimensions
being treated as valid on the Pi's non-X11/KMS path. It retains the existing SDL
display-size fallback. See [graphics validation](../../docs/pi5-graphics-validation.md)
for the failing draw bounds, successful GPU capture and regression results.

`0003-brand-ver-output.patch` changes the default `VER` output and help to
Pi-DOS branding. Both `VER` and `VER /R` start with the Pi-DOS version from
the root `VERSION` file, also used by the startup screen. The native and
Buildroot runtime builds generate `include/pi_dos_version.h` from that file.
The reported DOS compatibility version remains configurable; `VER /R` prepends
a Pi-DOS version line to the complete original DOSBox-X version and build output.

`0006-brand-system-drive-label.patch` changes the built-in Z: volume label from
`DOSBOX-X` to `PI-DOS`. The virtual drive returns a fixed label, so startup's
`LABEL` command cannot rename it. C: receives the `SYSTEM` label through the
supervisor's `MOUNT -label` option. D: is left available for CD-ROM images.

The native build helper applies `*.patch` in filename order, recognizes already
applied patches, and rejects mismatches. Upstream source archives retain their
original hashes; this patch set is a separate input to the resulting runtime.
Preserve the upstream component's license and notices when distributing changes.

`0008-stable-midi-destinations.patch` resolves the persisted `pidos:` MIDI
destination through the shared ALSA scanner in `src/common/midi_ports.h`.
It lists machine-readable IDs for PIDOSCFG and uses subscriptions so an
unplugged destination cannot redirect messages to a reused client number.
Managed profiles disable MIDI if their backend fails to open. Internal GM
uses the existing `synth` FluidSynth handler, which feeds DOSBox's SYNTH mixer
channel; Munt feeds MT32. Both backends are enabled in the ARM64 build.

`0009-supervised-ctrl-alt-delete.patch` intercepts Ctrl+Alt+Delete in SDL2 before
the guest keyboard reset. `src/common/reboot_shortcut.h` requests the existing
PID 1 SIGINT reboot protocol and consumes repeat/Delete-release events.
After a successful request, quit confirmation is bypassed for the supervisor's
shutdown. Only the opted-in direct child of PID 1 enables this behavior.
The native and Buildroot helpers install the shared header before compiling.

`0010-supervised-poweroff.patch` lets PID 1 bypass guest quit warnings after
creating `/run/pi-dos-stop` and sending SIGTERM. This keeps power-button
shutdown on the normal runtime teardown path while a game is running.
The shared helper accepts the marker only for the supervised child of PID 1.

`0007-preserve-overlapping-fpu-register-bits.patch` marks `RegBit` views as
aliasing types. GCC 14 on AArch64 otherwise loses the x87 stack push when
`FPTAN` or `FSINCOS` subsequently clears C2. Wolfenstein 3D builds incorrect
trigonometric tables with both the interpreter and dynrec cores. See the
[investigation and regression](../../docs/wolf3d-corruption.md).

The KMS raster/refresh patch requests the surface renderer's final dimensions
instead of its intermediate VGA dimensions. Exact-size HDMI modes are selected
at the advertised refresh closest to the guest's rate. This avoids carrying
the preceding mode's refresh into a game mode. Available modes, requested
dimensions and the presented surface/renderer/display sizes can be logged for
CRT diagnosis. The opt-in diagnostics patch uses the unified `[pidos] diagnostics`
setting in `C:\SYSTEM\PIDOS.CFG`, disabled by default. PID 1
passes the resolved setting to the emulator through the internal
`PI_DOS_DIAGNOSTICS` environment variable. Enabled capture writes to
`C:\SYSTEM\VIDEO.LOG` when no explicit `[log] logfile` is specified. With
diagnostics off, the verbose tracing and presentation queries are skipped.
Other SDL video backends retain their selection behavior.

`0011-fixed-lcd-monitor.patch` adds a managed SDL2 surface path for fixed LCD
output. PID 1 validates the manual mode and passes it through `PI_DOS_LCD_MODE`;
the host resolution/refresh no longer follows guest mode changes in that profile.
A separate RGB source surface receives the DOS raster, then
fits into a centered 4:3 or original-pixel viewport using `src/common/lcd_video.h`.
The existing SDL surface/GLES presentation handles the final window surface.
Guest video timing and the existing CRT mode-switch path remain separate. This patch needs physical
LCD validation; host checks cover configuration and viewport geometry.
