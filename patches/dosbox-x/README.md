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

The native build helper applies `*.patch` in filename order, recognizes already
applied patches, and rejects mismatches. Upstream source archives retain their
original hashes; this patch set is a separate input to the resulting runtime.
Preserve the upstream component's license and notices when distributing changes.

The KMS raster/refresh patch requests the surface renderer's final dimensions
instead of its intermediate VGA dimensions. Exact-size HDMI modes are selected
at the advertised refresh closest to the guest's rate. This avoids carrying
the preceding mode's refresh into a game mode. Available modes, requested
dimensions and the presented surface/renderer/display sizes can be logged for
CRT diagnosis. The opt-in diagnostics patch adds `[sdl] videodiagnostics=false`
as the default. Enabling it at startup writes to `C:\SYSTEM\VIDEO.LOG` when
no explicit `[log] logfile` is specified. With diagnostics off, the verbose
tracing and presentation queries are skipped. Other SDL video backends retain
their selection behavior.
