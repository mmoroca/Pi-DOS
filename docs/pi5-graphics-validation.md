# Raspberry Pi 5 — KMS/DRM and GLES graphics validation

21 October 2026. **Passed over SSH with native SDL2, hardware GLES and the
Pi-DOS display-dimension patch.** No ReplayOS service was stopped.

## Observed rendering path

```text
DOS mode 13h VGA emulation
  → DOSBox-X surface output / CPU scaling
  → SDL2 streaming texture and opengles2 renderer
  → Broadcom V3D / OpenGL ES 3.1
  → EGL / GBM / KMS
  → completed DRM page flips
```

| Observation | Result |
| --- | --- |
| Board / kernel | Pi 5, AArch64, `6.18.39+rpt-rpi-v8`, 4096-byte pages |
| HDMI device / mode | `/dev/dri/card1`, HDMI-A-1, 1920×720 |
| SDL renderer | `opengles2`, flags `10` including `SDL_RENDERER_ACCELERATED` |
| GL API | `OpenGL ES 3.1 Mesa 26.2.2-1~bpo13+0~rpt1` |
| GPU / vendor | `V3D 7.1.10.2` / Broadcom |
| GPU capture | Eight expected VGA color bars matched in order and width |
| Captured display rectangle | `(640,120)`–`(1280,600)`: 640×480, exactly 4:3 |
| Presentation trace | 64 swap calls, 62 completed DRM flip callbacks; 54 after capture |
| DOS completion / process exit | `ARMDOS_GRAPHICS_OK` / status 0 |
| Display release | No process holding DRM card devices after exit |

The image is centered with black borders on the wide 1920×720 display. VGA
emulation and scaling still run on the CPU; texture rendering/presentation use
the GPU. The observed call stack connects VGA drawing to `SDL_UpdateWindowTexture`,
`SDL_RenderPresent_REAL` and `KMSDRM_GLES_SwapWindow`.

## Hardware integration bug found and fixed

The original binary initialized KMS but failed the sustained VGA test. The
failure reproduced without the capture helper. GDB showed fullscreen dimensions
of `0×0` and a draw rectangle of `(-320,-240,640,480)`, followed by heap corruption
and an abort when SDL recreated the window texture during shutdown.

`ScreenSizeInfo` uses `-1` for unknown display dimensions. Two upstream checks
accepted any nonzero value, including that sentinel. Converting it to unsigned
dimensions on this ARM64 path produced invalid sizes. Supplying an explicit
HDMI resolution alone did not fix it: the resize handler overwrote those values.

[The minimal patch](../patches/dosbox-x/0001-reject-invalid-display-dimensions.patch)
changes both checks to require positive dimensions. The existing SDL display-mode
fallback then supplies the real dimensions. This fixes the invalid draw offsets
without replacing the DOS engine or adding a custom renderer.

The build helper applies the patch idempotently with zero fuzz, fails on source
mismatch, and records its SHA-256 under `output/logs/dosbox-x-patches.sha256`.
This is the first Pi-DOS source patch to DOSBox-X. Earlier Phase 0 builds used
unmodified upstream sources.

The original default renderer selected desktop OpenGL 3.1 on V3D. The working
GLES test explicitly selects `opengles2` using SDL2's
`SDL_FRAMEBUFFER_ACCELERATION` and `SDL_RENDER_DRIVER` hints. It also supplies the
connected display's advertised resolution and enables DOS aspect correction.

## Repeating the test on the Pi

From an SSH session:

```sh
cd /opt/pi-dos
make smoke-graphics
```

The diagnostic defaults to GLES. It checks that DRM card devices are unused,
selects the connected HDMI card and advertised mode, and refuses to stop any
existing display service. `DRM_DEVICE_INDEX` and `HDMI_MODE` can select a specific
card/mode; the mode must be advertised by the connected output.

It assembles [bars.asm](../tests/video/bars.asm), starts DOSBox-X under GDB with
default configuration, and exits automatically. The fixture draws 320×200 VGA
bars, changes a corner pixel to keep frames updating, and holds the pattern for
about three seconds after capture acknowledgment. The entire debugger run is
bounded to 45 seconds with a subsequent kill deadline.

The diagnostic-only preload helper reads the current GPU back buffer just
before a KMS swap. GDB counts the native SDL2 swap and DRM completion handlers.
The validator checks colors, geometry, hardware/GLES identity, DOS output, clean
exit and release of DRM devices. GDB and internal SDL symbol names are tied to
the pinned builds; this helper is not a runtime dependency of Pi-DOS.

## Evidence and limits

Saved locally and on the Pi under `output/reports/pi5/artifacts/graphics/`:

- `frame.ppm`: GPU readback; a local `frame.png` conversion is also available.
- `renderer.json`: actual renderer, GPU and API strings.
- `presentation.json`: swap and completion counts.
- `result.json`: color/aspect checks and device-release result.
- `run.log`: debugger call stack and DOSBox-X output.

`graphics-before-patch.log` in the parent report records the negative draw bounds
and crash. `environment.json` and `checks.json` include the applied patch identity.
The existing COM/EXE/BAT tests passed again with the normal interpreter and with
debugger-confirmed ARM64 dynrec after rebuilding the patched binary.

This demonstrates one VGA mode, GPU-rendered pixels and completed KMS
presentation on the connected display. It is not a camera observation of the
physical panel, a performance/VSync benchmark, or proof of every DOS video mode.
Physical keyboard/mouse and broader mode-switch compatibility remain separate
checks. Subsequent [virtual keyboard](pi5-keyboard-validation.md) and
[ALSA audio](pi5-audio-validation.md) reports record their own evidence and limits.
