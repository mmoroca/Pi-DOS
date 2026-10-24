# Archived Phase 0 — SDL3 findings

Recorded 21 October 2026. Superseded by the decision to use native SDL2.
This is a historical report; current commands and status are in [phase0.md](phase0.md).

## Selected candidates

| Component | Revision | Selection |
| --- | --- | --- |
| DOSBox-X | `4f19017c5f565dc40d01fede0f1382892e7243d7` | Upstream `dosbox-x-v2026.08.31` tag |
| SDL3 | `d0183f0ee3b2497f49c6322c62354d114c22af29` | Pinned main snapshot identifying as 3.5.0 |

These historical candidates are no longer the active dependency pins.
Buildroot, kernel, firmware and Mesa release pins remain to be selected at the
appropriate gate.

## Observed on the development host

Host architecture: **x86-64 Linux**. No Raspberry Pi hardware was exercised.
The installed system SDL3 reports 3.4.2. The workflow instead builds a private
SDL 3.5.0 snapshot, leaving system packages untouched.

| Check | Result |
| --- | --- |
| Both source archive SHA-256 hashes | Verified |
| Private native SDL3 shared library | Built and installed under `output/host/` |
| SDL3 KMS/DRM, GLES2, ALSA, Linux evdev compile support | Enabled in generated build configuration |
| SDL3 X11 and Wayland compile support | Disabled |
| DOSBox-X Autotools generation | Passed |
| DOSBox-X configure with `--enable-sdl3` | Passed; generated `C_SDL3=1` |
| Unmodified DOSBox-X compilation | Failed: legacy `SDL_video.h` include not found |
| Video compilation with corrected SDL3 include search paths | Failed: removed/changed SDL APIs |
| DOS prompt, DOS applications, ARM64 dynrec, hardware output/input/audio | Not validated |

`SDL3_VERSION=3.5.0` appears in the pinned DOSBox-X `configure.ac`. Its
`AM_PATH_SDL3` macro checks for the `sdl3` pkg-config module but does **not**
actually enforce that minimum. The build helper conservatively selects and
checks 3.5.0 rather than silently accepting a different system SDL3.

The source's `include/dosbox.h` defines `C_SDL2` internally when `C_SDL3` is set,
explicitly as a porting aid. That preprocessor symbol is not evidence that an
SDL2 runtime is linked. A future successful binary needs both native SDL3 link
verification and execution checks.

## Concrete compilation gaps

The unmodified build uses flat includes such as `<SDL_video.h>`, while SDL3's
pkg-config flags expose the parent include directory for `<SDL3/SDL_video.h>`.
To isolate this from API migration, a diagnostic compile added both
`output/host/include` and `output/host/include/SDL3` to the include search path.
The CPU translation unit then compiled, but `src/gui/sdlmain.cpp` failed on
examples including:

- `SDL_RWops` in bundled audio decoder interfaces; SDL3 uses `SDL_IOStream`.
- `SDL_mutex`; SDL3 uses `SDL_Mutex`.
- `SDL_CreateRGBSurfaceFrom` and `SDL_FillRect`.
- Direct use of old surface format fields such as `format->BitsPerPixel`.

These are representative errors, not an exhaustive porting inventory. A source
review also shows the old audio-device callback/specification and locking API
in `src/hardware/mixer.cpp`; its SDL3 migration remains to be implemented and
compiled. No speculative source patches or SDL2 compatibility shim were added.

Logs retained locally:

```text
output/logs/sdl3-configure.log
output/logs/sdl3-build.log
output/logs/sdl3-install.log
output/logs/dosbox-x-autogen.log
output/logs/dosbox-x-configure.log
output/logs/dosbox-x-build.log
output/logs/dosbox-x-header-diagnostic.log
output/logs/dosbox-x-video-diagnostic.log
```

The active build helper now builds SDL2. The SDL3 build artifacts and logs from
this investigation remain separate from the SDL2 build. These findings do not
describe the current SDL2 baseline.

## Deferred SDL3 migration

Port native SDL3 headers, surfaces/windows/rendering, input events/scancodes,
timers, I/O streams, synchronization and audio streams. Audit bundled decoders
and host integrations that depend on older SDL. Keep changes as a reviewable
patch series, and retain the existing DOS/CPU/BIOS implementation. Do not enable
an SDL2 compatibility runtime to get the build to pass.

After compilation succeeds, check the linked libraries for native SDL3 and run
the following on ARM64. Record exact source revisions, compiler, board, kernel,
page size and CPU core for every result.

- [ ] Reach the built-in DOS shell using native SDL3.
- [ ] Run DIR, CD and COPY in a dedicated scratch DOS drive.
- [ ] Execute redistributable small COM and EXE programs.
- [ ] Execute a BAT script with variable expansion.
- [ ] Repeat execution with the interpreter and ARM64 dynrec.
- [ ] Confirm ARM64 dynrec was compiled and actually selected, without fallback.
- [ ] Investigate executable-memory, instruction-cache and self-modifying-code behavior.

Phase 1 then requires actual DRM master acquisition, GLES rendering on HDMI,
keyboard/mouse enumeration and hotplug, and ALSA playback without a desktop.
Compiled SDL backends alone establish none of those runtime properties.

## Primary references

- [Pinned DOSBox-X configure.ac](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/configure.ac)
- [Pinned DOSBox-X SDL detection](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/acinclude.m4)
- [Pinned DOSBox-X porting aid](https://github.com/joncampbell123/dosbox-x/blob/4f19017c5f565dc40d01fede0f1382892e7243d7/include/dosbox.h)
- [Pinned SDL3 CMake configuration](https://github.com/libsdl-org/SDL/blob/d0183f0ee3b2497f49c6322c62354d114c22af29/CMakeLists.txt)
- [Pinned SDL3 migration guide](https://github.com/libsdl-org/SDL/blob/d0183f0ee3b2497f49c6322c62354d114c22af29/docs/README-migration.md)
