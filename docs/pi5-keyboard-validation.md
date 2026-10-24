# Raspberry Pi 5 — SSH-only keyboard validation

21 October 2026. **Passed with a temporary Linux uinput keyboard, native SDL2
KMS/DRM and the normal DOSBox-X interpreter.** No ReplayOS service was stopped;
no package, kernel or system configuration change was needed.

```text
Host test script → /dev/uinput → Linux evdev device
  → SDL2 KMS/evdev → DOSBox-X keyboard emulation
  → BIOS INT 16h and DOS INT 21h
```

The test created `/dev/input/event11` named `Pi-DOS diagnostic keyboard 16397`.
DOSBox-X had that device open before injection. The only other process holding
it was the existing `systemd-logind`; no ReplayOS application held it.

The original [DOS fixture](../tests/dos/keyboard.asm) signals readiness, records
nine BIOS key words, then reads three characters with DOS function `INT 21h/AH=08h`.
The host compares the complete 21-byte result with the expected sequence:

| Input | BIOS AX, hexadecimal |
| --- | --- |
| a | `1e61` |
| b | `3062` |
| Shift+A | `1e41` |
| a after releasing Shift | `1e61` |
| 1 | `0231` |
| Backspace | `0e08` |
| Enter | `1c0d` |
| F1 | `3b00` |
| Left Arrow | `4b00` |

The DOS character reads returned exactly `xY3`. The completion marker was
`ARMDOS_KEYBOARD_OK`, the emulator exited with status 0, the virtual device was
removed, and no process retained a DRM card descriptor after exit.

## Repeat over SSH

As root on the Pi, with its connected HDMI display unused:

```sh
cd /opt/pi-dos
make smoke-keyboard
```

The [host diagnostic](../tools/phase0/keyboard.py) refuses to take an occupied
DRM device. It creates the keyboard before SDL's initial device scan, waits for
DOS readiness and confirms DOSBox-X opened the event node before injecting keys.
It refuses injection if another application besides `systemd-logind` has opened
that virtual node. It uses default DOSBox-X configuration, a US DOS keyboard
layout, GLES surface presentation, dummy audio and a fresh scratch C: drive.

Startup and completion waits are bounded. Cleanup terminates a stalled emulator
and destroys the uinput device, including on failure. The existing development
Pi's udev service classifies the device; this does not validate discovery without
udev in the future appliance image.

## Evidence and limits

The successful run is `output/smoke/keyboard.5s9wmia1` on the Pi. Evidence is
copied locally and remotely to `output/reports/pi5/artifacts/keyboard/`:
`KEYS.BIN`, `KEYBOARD.OUT`, `KEYBOARD.COM`, `run.log` and `result.json`.
`output/reports/pi5/checks.json` records the result and scope.

This validates the kernel-event-to-DOS software path and the tested modifier,
character, function and cursor keys. It does not exercise a physical USB
keyboard, hotplug after SDL startup, other layouts, key repeat, Ctrl/Alt/Break
semantics or mouse input. The subsequent [ALSA playback check](pi5-audio-validation.md)
has its own evidence and limitations.
