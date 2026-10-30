# Phase 0 — Native SDL2 baseline

Updated 21 October 2026. Native SDL2 is the selected platform for the initial
release. SDL3 is deferred until DOSBox-X has a complete native port and passes
the same integration and hardware gates.

## Selected sources

| Component | Revision | Selection |
| --- | --- | --- |
| DOSBox-X | `4f19017c5f565dc40d01fede0f1382892e7243d7` | Upstream `dosbox-x-v2026.08.31` tag |
| SDL2 | `5d249570393f7a37e037abf22cd6012a4cc56a71` | Native `release-2.32.10` tag |

Archives are pinned by SHA-256 in [sources.lock.json](../sources.lock.json).
SDL2 is built from the upstream SDL repository, without `sdl2-compat` or SDL3.
Buildroot, kernel, firmware and Mesa release pins remain to be selected at their
validation gate.

## Development build

The build helper installs private SDL2 under `output/host-sdl2/`, selects
`--enable-sdl2`, and builds DOSBox-X in `output/build/dosbox-x-sdl2/`. It asserts
that the generated configuration enables `C_SDL2` and leaves `C_SDL3` undefined.
Previous SDL3 artifacts remain in separate directories.

```sh
make fetch
make sdl2
make phase0
# Once the pinned archives are cached:
bash tools/phase0/build.sh --offline
```

The SDL2 build enables KMS/DRM, OpenGL ES, ALSA and Linux evdev. X11, Wayland,
PulseAudio, PipeWire, JACK and sndio are disabled. The helper verifies required
backend macros rather than relying only on requested CMake options.

Initial host validation used x86-64 Linux. The same pinned baseline has now
been built and tested headlessly on Pi 5 ARM64; see the
[Pi 5 validation report](pi5-validation.md) for environment and evidence.

| Check | Result |
| --- | --- |
| Pinned SDL2 and DOSBox-X archive hashes | Verified |
| Private native SDL2 configuration, compilation and install | Passed |
| KMS/DRM, GLES2, ALSA and evdev build macros | Enabled |
| X11 and Wayland build macros | Disabled |
| DOSBox-X configure and full native build | Passed; `C_SDL2=1`, `C_SDL3` undefined |
| Executable `-version` | Passed; reports `DOSBox-X version 2026.08.31 SDL2` |
| ELF and loader dependency checks | Loads `output/host-sdl2/lib/libSDL2-2.0.so.0`; no SDL3 or missing shared libraries detected |
| Minimal headless DOS shell startup | Passed on x86-64: mount C:, write RESULT.TXT from shell, exit successfully |
| Small original COM/EXE/BAT fixtures | Passed on x86-64 normal interpreter, including an MZ relocation, child exit codes and variable expansion |
| Basic DIR/CD/COPY through a BAT script | Passed on x86-64; host verifies file contents and directory listing |
| Pi 5 native ARM64 build and headless fixtures | Passed with interpreter and dynrec; actual translated-block execution observed with GDB |
| SSH-launched KMS/DRM startup | Backend initialization and scripted DOS execution passed; visual output unobserved |
| GPU-readback VGA pattern, GLES rendering and KMS flips | Passed on Pi 5 with the display-dimension patch; see the graphics report |
| Linux uinput → SDL2 evdev → DOS keyboard | Passed on Pi 5: BIOS scan codes, DOS characters and Shift press/release |
| DOS tones → SDL2 ALSA → HDMI playback | Short check passed: PC speaker, OPL2 and SB16 DMA; one teardown XRUN recorded |
| Physical input, audible speaker output and appliance boot | Not yet validated |

No DOSBox-X source patches were needed for the initial headless SDL2 build.
Subsequent [graphics validation](pi5-graphics-validation.md) required a minimal
display-dimension patch, now applied by the build helper. Broader compatibility,
physical input and long-duration audio/timing gates remain open.

The immediate host startup check is `make smoke`. It uses `-defaultconf`, a
fresh directory under `output/smoke/`, software surface output and dummy SDL2
video/audio. The saved result proves the DOS shell executed a command and wrote
through a mounted drive. This does not validate graphics, input, audio or DOS
application compatibility. No visible window is expected.

Launching the bare executable may load a personal configuration selecting X11.
That fails with `Can't init SDL x11 not available` because this build disables
X11/Wayland. The smoke command isolates startup from those personal settings.

The next check after startup is `make smoke-programs` (requires NASM and Python
3). It assembles the original fixtures in `tests/dos/`, runs them through a BAT
script using `core=normal`, and validates their output, exit-code checks, batch
variables, directory navigation and copied file contents. Each run uses a fresh
directory under `output/smoke/` and retains `run.log`. This has passed on the
x86-64 host and Pi 5; it covers small real-mode fixtures rather than general DOS
compatibility. The BAT script is invoked with `CALL` so control
returns to the startup command stream for a clean exit.

`CPU_CORE=dynamic_rec make smoke-programs` requests the compiled dynrec core.
Adding `VERIFY_DYNREC=1` runs under GDB and requires evidence that the dynrec
decoder was selected and returned from a translated block. That check passed
on Pi 5. The debugger breakpoints are specific to the pinned source revision.

Logs are under `output/logs/`: `sdl2-configure.log`, `sdl2-build.log`,
`sdl2-install.log`, `dosbox-x-sdl2-configure.log`, `dosbox-x-sdl2-build.log`,
`dosbox-x-sdl2-version.log` and `dosbox-x-sdl2-linkage.log`.

## ARM64 and hardware acceptance

The basic native ARM64 execution checks below have passed on Pi 5. Extended JIT
validation remains open. The report records board, kernel, page size, compiler,
source revisions and CPU core.

- [x] Reach the built-in DOS shell using native SDL2 (headless).
- [x] Run DIR, CD and COPY in a dedicated scratch DOS drive.
- [x] Execute original COM and EXE programs and a BAT script.
- [x] Repeat execution with the interpreter and ARM64 dynrec.
- [x] Confirm ARM64 dynrec was compiled, selected and executed translated code.
- [ ] Validate executable memory, cache synchronization and self-modifying code.

Phase 1's first GLES/frame-presentation check has passed on the Pi through the
native KMS backend. The [virtual keyboard test](pi5-keyboard-validation.md) also
passed through Linux uinput and SDL2 evdev into BIOS and DOS services. The
[audio check](pi5-audio-validation.md) captured PC speaker, OPL2 and Sound Blaster
DMA tones while real ALSA HDMI hardware playback advanced. It recorded one XRUN
after fixture completion; shutdown ordering remains open. Physical keyboard/mouse
behavior and hotplug, audible speaker output, long-duration audio reliability,
additional video modes and timing behavior still require validation. Compiled
backends alone do not establish runtime hardware behavior.

The planned appliance environment uses SDL2's variable names:

```sh
SDL_VIDEODRIVER=kmsdrm
SDL_AUDIODRIVER=alsa
```

## Buildroot image milestone

The first Pi 5 SD image has now been generated and passed static image checks.
It packages the kernel, custom C PID 1 and SDL2 DOS runtime with a separate
then-named `DOSDATA` partition (now `PI-DOS`). This does not establish a successful
appliance boot or
persistence across reboots. See the [image report](buildroot-pi5.md).
Mouse validation and the audio teardown investigation are deferred at the
user's request while moving to this image milestone.

## Future SDL3 upgrade

The [archived SDL3 investigation](phase0-sdl3-history.md) documents the failed
initial porting baseline. Upgrade only after upstream DOSBox-X builds and runs
with native SDL3, then repeat DOS execution, interpreter/dynrec, KMS/DRM/GLES,
input and audio validation on the supported boards. Do not introduce
`sdl2-compat` as an intermediate runtime dependency.

## Primary references

- [Pinned DOSBox-X build configuration](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/configure.ac)
- [Pinned DOSBox-X SDL detection](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/acinclude.m4)
- [Pinned SDL2 CMake configuration](https://github.com/libsdl-org/SDL/blob/5d249570393f7a37e037abf22cd6012a4cc56a71/CMakeLists.txt)
