# Raspberry Pi 5 — SSH-only ALSA audio validation

21 October 2026. **The short DOS tone/playback check passed through native SDL2
ALSA to HDMI.** One hardware XRUN observation occurred after the DOS fixture
completed, during teardown; no XRUN was sampled before completion. Audible sound
from the display's speakers has not been verified by a listener.

No ReplayOS service was stopped, no packages were installed, and no system audio
configuration was changed. Video used the previously validated KMS/GLES path.

## Observed path

```text
Original DOS COM fixture: PIT/speaker, OPL2 registers, SB16 ISA DMA
  → DOSBox-X sound emulation and mixer
  → native SDL2 ALSA backend
  → temporary ALSA file PCM (WAV capture + real hardware slave)
  → plughw:CARD=vc4hdmi0,DEV=0
  → /dev/snd/pcmC0D0p → HDMI
```

The connected display advertises two-channel LPCM at 32, 44.1 and 48 kHz.
The test explicitly selects its ALSA card, bypassing the ReplayOS default PCM.
The file PCM sends samples to that hardware device while recording the same
stream; its slave is not a null device. Configuration applies only to the test
process through `ALSA_CONFIG_PATH` and `AUDIODEV`.

| Check | Result |
| --- | --- |
| Backend | SDL reported `Audio alsa`, `Video KMSDRM` |
| Capture format / duration | WAV, 48 kHz, stereo, signed 16-bit PCM; 9.984 seconds |
| PC speaker via PIT channel 2 | Expected ~439.96 Hz; representative measured crossing rate 440 Hz |
| AdLib / OPL2 | Expected ~659.99 Hz; representative measured crossing rate 660 Hz |
| Sound Blaster 16 | 8-bit mono unsigned auto-init DMA, 16 kHz source; captured 1 kHz tone |
| Signal validation | Expected fundamental energy, stereo signal, tone order and silent intervals matched |
| Kernel playback | 199 `RUNNING` observations owned by DOSBox-X, 199 distinct advancing hardware pointers |
| Hardware parameters | 48 kHz, stereo, `IEC958_SUBFRAME_LE`; period 1024, buffer 2048 frames |
| Sampled underruns | None before fixture completion; one XRUN observation during teardown |
| DOS completion / process exit | `PI_DOS_AUDIO_OK` / status 0 |
| Device release | No remaining PCM or DRM card owners |

The capture is taken before ALSA's conversion to HDMI IEC958 subframes. Its WAV
header confirms the application-side format; `/proc/asound` supplies separate
evidence that the actual hardware PCM was owned by DOSBox-X and running.

## Repeat over SSH

On the Pi with the connected HDMI display and audio PCM unused:

```sh
cd /opt/pi-dos
make smoke-audio
```

The [diagnostic](../tools/phase0/audio.py) assembles the original
[audio.asm](../tests/dos/audio.asm), uses default DOSBox-X configuration and
mapper settings, and mounts a fresh scratch C: drive. It runs the normal
interpreter at fixed 20000 cycles, with a 48 kHz mixer, 1024-frame blocks,
30 ms prebuffer, and 25% emulator master volume. Each tone lasts approximately
two seconds, separated by approximately one-second silent intervals. No
keyboard input is needed.

It refuses to take an occupied DRM card or selected ALSA playback device,
requires exactly one connected HDMI display and one HDMI ALSA card advertising
audio, and bounds the emulator run to 30 seconds plus termination deadlines.
The temporary ALSA configuration remains only in the scratch evidence directory.
No listening or recording hardware is required.

The fixture creates `ACTIVE.OK` at start and `DONE.OK` after the final silent
interval. Hardware status is sampled about every 50 ms. XRUNs before completion
fail the check; XRUN observations after completion are recorded separately.
The completion marker establishes the boundary without assuming that a late
underrun occurred during a tone.

## Evidence and remaining scope

The successful Pi run is `output/smoke/audio.pddixagc`. Evidence is copied locally
and remotely under `output/reports/pi5/artifacts/audio/`:

- `pcm.wav`: the ALSA PCM capture.
- `hardware-status.json`: timestamped status, ownership, hardware pointers and parameters.
- `signal.json`: per-window signal measurements and tone checks.
- `result.json`, `AUDIO.OUT`, `run.log`, `eld.txt`: summary, DOS completion, backend and display capability evidence.
- `audio.asm`, `audio.py`, `AUDIO.COM`: diagnostic source and executable.

`output/reports/pi5/checks.json` records the successful result, diagnostic hashes
and limits. Earlier runs exposed a diagnostic BIOS-timing error and a log
assertion issue, which were corrected. A subsequent run exposed the late XRUN;
the final run explicitly verified that it occurred after fixture completion.

This is a short playback-path check, not a listening test, HDMI receiver
loopback, latency measurement or long-duration reliability result. Polling can
miss brief recovered underruns. Shutdown audio ordering needs further review
before release because the hardware reached XRUN during teardown. Sound
Blaster digital playback here covers one 8-bit mono DMA mode, not the complete
SB16 feature set or game compatibility. AV synchronization, physical speaker
output, mouse input and appliance-image boot remain separate work.
