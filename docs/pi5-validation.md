# Raspberry Pi 5 — Phase 0 headless validation

Recorded 21 October 2026. **The native ARM64 SDL2 baseline passes the small DOS
fixtures with the interpreter and ARM64 dynrec.** Subsequent
[graphics validation](pi5-graphics-validation.md) also passed GPU readback,
GLES rendering and KMS presentation after a display-dimension fix. The virtual
keyboard path and short ALSA HDMI tone check have also passed. Physical input,
audible speaker output and long-duration audio behavior remain unvalidated.

This Pi is a ReplayOS development environment accessed exclusively through SSH.
Tests must be scripted and bounded; no local console interaction is assumed.
The initial SSH-launched KMS startup check below was followed by an automated
color-pattern and page-flip test. Physical input remains unobserved.

## Environment

| Item | Observed value |
| --- | --- |
| Board | Raspberry Pi 5 Model B Rev 1.0, 2 GB RAM |
| OS | Debian GNU/Linux 13 (trixie), AArch64 |
| Kernel | `6.18.39+rpt-rpi-v8` |
| Host page size | 4096 bytes |
| Compiler | GCC 14.2.0, Debian `14.2.0-19` |
| CMake / NASM | 3.31.6 / 2.16.03 |
| Project directory | `/opt/pi-dos` |
| Build parallelism | `JOBS=2` |

The build used the same checksum-verified archives in
[sources.lock.json](../sources.lock.json): DOSBox-X revision
`4f19017c5f565dc40d01fede0f1382892e7243d7` and native SDL2 2.32.10 revision
`5d249570393f7a37e037abf22cd6012a4cc56a71`. NASM was the one missing dependency
installed on the Pi. The initial headless baseline required no DOSBox-X source
patches; later graphics validation introduced the tracked display-dimension
patch. The report metadata now includes that patch identity.

## Results

| Check | Result |
| --- | --- |
| Native SDL2 and DOSBox-X compilation | Passed |
| Executable architecture | ELF64, AArch64 |
| SDL selection and linkage | `C_SDL2=1`; loads private `/opt/pi-dos/output/host-sdl2/lib/libSDL2-2.0.so.0`; no SDL3 or missing dependencies detected |
| CPU build configuration | `C_TARGETCPU ARMV8LE`, `C_DYNREC=1` |
| SDL backends | KMS/DRM, GLES2, ALSA and evdev compiled; X11/Wayland disabled |
| `make smoke` | Passed: shell startup, mounted C:, file write, clean exit |
| `CPU_CORE=normal make smoke-programs` | Passed: COM, relocated MZ EXE, exit codes, BAT variables, DIR/CD/COPY |
| `CPU_CORE=dynamic_rec VERIFY_DYNREC=1 make smoke-programs` | Passed: same fixtures; debugger confirmed selection and execution of dynrec |
| SSH-launched KMS/DRM startup | Passed: SDL reported `Video KMSDRM`, DOS wrote `ARMDOS_KMS_OK`, process exited with status 0 |
| GLES VGA pattern and KMS presentation | Passed after the dimension fix: V3D / GLES 3.1, correct colors and 4:3 geometry, 62 completed flips |
| Virtual keyboard → SDL2 → DOS | Passed: BIOS scan codes, DOS characters and Shift press/release |
| DOS tones → SDL2 ALSA → HDMI playback | Short check passed: PC speaker, OPL2 and SB16 DMA, advancing hardware pointer; one teardown XRUN observation |

All three execution checks exited with status 0. The version-only command
printed `DOSBox-X version 2026.08.31 SDL2` but exited with status 1 on the Pi.
The pinned upstream command parser returns false after printing version
information, and `main` returns 1 for that early return. The version banner was
recorded separately from the successful runtime checks.

## Dynrec evidence

The optional debugger check in [dynrec.gdb](../tools/phase0/dynrec.gdb) observed:

```text
ARMDOS_DYNREC_ENTRY_REACHED
ARMDOS_DYNREC_DECODER_MATCH=1
ARMDOS_DYNREC_BLOCK_RETURN_REACHED
#0 CPU_Core_Dynrec_Run() at core_dynrec.cpp:325
```

The first breakpoint checks decoder selection on entry. The second breakpoint
is after `core_dynrec.runcode(...)` returns, demonstrating that a translated
code block ran. This is stronger evidence than merely requesting a core in a
configuration file. It does not imply every guest instruction avoids the
engine's ordinary interpreter fallback paths.

## Repeating the execution checks

The Pi is already built. From its configured project directory:

```sh
cd /opt/pi-dos
make smoke
make smoke-programs
CPU_CORE=dynamic_rec VERIFY_DYNREC=1 make smoke-programs
```

The last command requires GDB and debug symbols, both present on this Pi.
`CPU_CORE` defaults to `normal`; `VERIFY_DYNREC=1` checks actual execution using
breakpoints tied to the pinned DOSBox-X source. Recheck the source-line
breakpoint when updating DOSBox-X.

Each test creates a fresh directory under `output/smoke/`. No display window is
expected: SDL2 video/audio use dummy drivers and startup ignores personal
DOSBox-X configuration.

## Evidence and remaining scope

Environment metadata, result summaries, build logs, generated configuration and
successful test artifacts are saved under `output/reports/pi5/` on the Pi and
in the local project. Key files include `environment.json`, `checks.json`,
`dynrec.log` and `artifacts/dynrec/run.log`.

## SSH-only KMS startup check

Inspection found HDMI-A-1 connected on `/dev/dri/card1`, with no process holding
DRM devices. A 20-second-bounded, automatically exiting DOSBox-X run used native
KMS/DRM video, dummy audio, fullscreen surface output, default configuration and
scripted DOS commands. It initialized successfully, wrote the expected DOS
result and exited normally. No ReplayOS service was stopped. The DRM client
tables were empty again after exit.

Evidence is in `artifacts/kms-startup/run.log` and `checks.json`. This establishes
backend initialization and DOS execution over an SSH-launched KMS session. It
does not establish visible pixel correctness, sustained frame presentation,
GLES acceleration, physical keyboard/mouse behavior or ALSA playback.

This validates small real-mode DOS fixtures and basic ARM64 JIT execution on a
4 KB kernel. Protected-mode/DPMI software, extended JIT/cache/self-modifying-code
regressions, 16 KB host pages, additional graphics modes, physical keyboard/mouse,
audible speaker output, long-duration audio reliability, reboot persistence and
an appliance boot image remain unvalidated.

Frame presentation and the rendering path have now been tested; see the
dedicated graphics report. The [SSH-only keyboard check](pi5-keyboard-validation.md)
also passed: a temporary Linux uinput device delivered the expected BIOS scan
codes and DOS characters through SDL2 evdev, including Shift press/release.
The virtual device was removed and DRM released after normal exit. This does
not validate physical USB input or hotplug. The [ALSA audio check](pi5-audio-validation.md)
also passed for PC speaker, AdLib and SB16 DMA tones captured while HDMI hardware
playback advanced. One XRUN was sampled after fixture completion during teardown;
shutdown ordering needs further review. An interactive HDMI prompt has not been
visually observed by a person, and audible speaker output has not been verified.
