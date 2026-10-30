# Pi-DOS setup

`PIDOSCFG.EXE` is an original, standalone 16-bit DOS MZ program written in C89.
It runs in real mode with 8086 instructions, uses conventional memory, and
requires neither an extender, DPMI, an x87 nor a third-party UI framework.
It is bundled as `Z:\BIN\PIDOSCFG.EXE`, on the standard Pi-DOS PATH.

The 80x25 color/monochrome interface uses BIOS keyboard services and a small text-mode
widget layer. Mouse input uses the DOS INT 33h driver when present. Keyboard
navigation works without that driver. The program restores the previous 80x25 text
screen and cursor on exit. See [the DOS manual](README.DOS) for controls and
command-line use. The image installs that manual as `C:\DOCS\PIDOSCFG.TXT`.

Eight pages configure CPU, Memory, Video, Sound, Mixer, Input, Network and System settings. F5/F6
change pages; Tab/Shift+Tab move focus; Enter edits; F2 saves all pages; Esc
cancels. Selection dialogs support typing to filter, arrows and page keys.
**Restore defaults** (F3 or Alt+R) resets the current page only, pending Save.
The CPU default is Pentium 133 MHz (`pentium`, `dynamic_rec`, `fixed 71240`),
marked `(default)` in the profile list.
Existing installations retain their configuration until explicitly changed.

## Monitor output

System offers **Monitor type: CRT / LCD**. Fresh installations and Restore
defaults select CRT. LCD preferences default to 1024x768 and 4:3 aspect.
Select an LCD resolution from 640x480, 800x600, 832x624, 1024x768, 1280x720 or 1920x1080, as advertised by the supplied
EDID. The selected output mode stays fixed when DOS applications change modes;
SDL chooses its advertised refresh without changing the emulated video clock.
The output resolution is manual, without monitor-native detection.

LCD aspect can be **4:3** or **Original pixels**, which preserves the source
raster ratio (for example, 320x200 is 8:5). Both fit the screen with centered
black borders as needed. Guests larger than the output are reduced to fit.
**Linear filtering** enables SDL2 bilinear scaling for a smoother picture; it is
disabled by default, using nearest-neighbor scaling. These controls are shown
only for LCD, and their preferences survive switching to CRT. There is no integer scaling or refresh selector.

CRT retains native DOS raster/refresh switching, with no aspect correction or
software scaler. Existing configurations without `[pidos] monitor` keep their
previous advanced display settings until a monitor type is explicitly selected.
Selecting a profile makes it authoritative over low-level `[sdl]` and `[render]`
display settings. Save and reboot to apply. Physical LCD/CRT validation remains
necessary for the new profile path.

```ini
[pidos]
monitor=lcd
lcd_mode=1024x768
lcd_aspect=4:3
lcd_linear=false
```

## Memory, video, sound and system

System's **Audio output** selects **HDMI (default)**, **3.5 mm jack (Pi 3/4)**,
or **External USB DAC**. Save and reboot to apply. The setting is stored as
`[pidos] audio_output=hdmi|jack|usb` in `C:\SYSTEM\PIDOS.CFG` and controls all
emulated sound, including internal MIDI and CD audio. HDMI uses the primary
port (HDMI0 on Pi 4/5). The image enables the Pi 3/4 analogue driver and firmware
audio node; the jack option requires a board with that socket.

Connect a USB audio-class DAC before boot. Discovery waits up to five seconds
and selects a playback-capable USB card by ALSA ID, then its lowest playback
device number. With multiple USB audio outputs, the first ID alphabetically
wins. Missing outputs stay silent for that boot without switching to another
output. Physical jack/USB playback validation remains pending.

RAM choices are 1, 2, 4, 8, 16, 32, 64 and 128 MiB, with 16 MiB the default.
The Memory page lists every size directly, like CPU profiles, with keyboard
and mouse selection. Help explains compatibility problems above 16 MiB. Selecting RAM clears the
additional `memsizekb` override. Saving rejects RAM beyond the chosen CPU's
address range.

The Video page uses the same selection-list control as Sound: IBM CGA,
Hercules, Tandy, IBM EGA, IBM VGA, Tseng Labs ET4000, S3 Trio64 (default),
VESA VBE 3.0 (`svga_dosbox`), and VESA VBE 2.0 (`svga_dosbox_vbe2`). Video names
are shared with the boot summary in `src/common/video_profiles.c`. The UI also
runs in Hercules monochrome text mode so users can change away from that adapter.
A separate 3dfx Voodoo checkbox enables/disables software hardware emulation.
Defaults are `voodoo_card=software`, `voodoo_maxmem=true`, `glide=false`.
Toggling the checkbox writes the full preset, using `voodoo_card=false` when
unchecked. This control does not enable Glide passthrough. Video changes need
a reboot. Existing custom video/Voodoo settings are retained until edited.

Sound presets are None, Tandy, AdLib, Creative Game Blaster/CMS, Sound Blaster
2.0, Sound Blaster Pro 2, Sound Blaster 16 (default), and Gravis Ultrasound
Classic. Each preset enables its devices and disables competing managed cards;
MIDI remains independent. PC Speaker is the first Sound control, checked when
enabled (the default). Applicable controls expose I/O address, IRQ, DMA, SB16 high DMA,
FM mode, and GUS sample RAM/patch directory. AdLib and Tandy use fixed ports.
GUS patches are not bundled; programs that require them need a populated
`C:\ULTRASND\MIDI` directory. Custom multi-card configurations are preserved
until a preset is selected. The boot table reports it as DOS sound.

### MIDI

Sound also offers independent MIDI output: None (default), Roland SC-55,
Roland GM, Roland MT-32, Roland CM-32L and External USB MIDI. The two GM
profiles use the supplied SF2 banks through FluidSynth; MT-32 and CM-32L use
Munt with separate control/PCM ROM pairs. Internal synthesis joins DOSBox's
mixer and the selected physical audio output. It can accompany Sound Blaster
digital effects. Select the matching music device in each game's setup.

MIDI settings opens MPU-401 interface (Intelligent by default or UART),
I/O port (330h by default) and IRQ (automatic by default). External USB MIDI
also offers a destination picker and optional SysEx delay for older modules.
Connect the interface, open the picker, and press F5 or click Rescan to refresh.
The saved identity survives ALSA client-number changes; interfaces without a
unique serial number should stay on the same USB socket. Missing or ambiguous
destinations disable MIDI for that boot rather than choosing another device.
Boot waits up to five seconds for the saved USB MIDI destination to appear
after display and audio detection, allowing USB enumeration to finish.
External MIDI sends messages only: connect the module's own audio output to
speakers or a mixer. DOSBox's internal MIDI volume does not control that audio.

All selections live in `C:\SYSTEM\PIDOS.CFG`. Assets live under
`C:\SYSTEM\MIDI`: `SC55.SF2`, `GMROLAND.SF2`, and control/PCM ROMs in
`MT32` and `CM32L` subdirectories. PID 1 preserves existing asset files and
reports missing assets/devices in the DOS MIDI boot row. MIDI output or
interface changes require a reboot. Missing profile keys preserve legacy
manual MIDI configuration.

Stage the supplied assets before a future image build:

```sh
python3 tools/dos/midi-assets.py --source /home/rtomas/Datos/Emulation/bios/RePlay/bios/scummvm
```

The importer verifies `configs/midi-assets.json` hashes and stores files in
the ignored `output/midi-assets` cache. Build hooks package that cache into
the initramfs and data partition; ROMs and SF2 files are not stored in Git.

Mixer controls expose master, SB digital, FM, GUS, CMS, PC speaker, Tandy,
Tandy DAC, Disney/Covox and internal MIDI music. Values are left:right percentages
from 0 to 200; amplification above 100 can clip. Arrow keys adjust by 5%, or
choose a preset/custom stereo value. On first entering Mixer, the utility reads
the running hardware via `CONFIG -ALL -GET` and levels via `MIXER`. When selected
hardware matches, enabled channels update immediately through
`Z:\SYSTEM\MIXER.COM /NOSHOW`. The page shows LIVE or AFTER REBOOT. Cancel restores
the starting levels (at MIXER's whole-percent precision); Save keeps the preview.
Pending hardware changes suspend preview and restore the original live levels.
Unavailable live control falls back to saving for reboot. A writable `C:\TEMP`
or `C:\` is needed for an exclusively created, automatically removed query file.
Inactive channels apply when their hardware is enabled after reboot.

The System page selects keyboard layout, DOS country and an installed IANA
timezone. Searchable DOS catalogs beside the executable are generated from the
pinned runtime's country/layout definitions and packaged tzdata. There are no
large in-memory timezone arrays. Defaults remain Spanish (`sp`), Spain (`34`),
and `Europe/Madrid`. Country can also accept a custom country,codepage value;
custom timezones must exist in the installed catalog. Regional settings apply
after reboot, using the existing timezone/DOS startup handling. The Diagnostic
logs checkbox controls `[pidos] diagnostics`, disabled by default. Enabling it
captures the existing boot, runtime, network and video logs after reboot.

## Input and controllers

The Input page configures global DOS joystick hardware and controller options.
Save and reboot to apply. No controller mapper file or per-game profile is needed.

- **DOS joystick type:** defaults to two conventional two-axis/two-button
  joysticks. Four-axis, second-controller four-axis, Thrustmaster FCS and CH
  Flightstick modes remain available. In appliance mode, `auto` also means the
  stable two-axis type, regardless of how many controllers are currently present.
- **Controller layout:** Automatic uses SDL2's built-in mappings for recognized
  gamepads, then falls back to raw Linux axes/buttons for unknown devices. Raw
  bypasses normalization. Controllers still require a working Linux driver.
- **Directional control:** left stick plus D-pad (default), stick only, or D-pad
  only. Raw devices use their first hat for the D-pad. D-pad input controls the
  first two DOS axes; advanced modes also retain their emulated hat functions.
- **Swap first two buttons:** exchanges the first two actions globally. The
  automatic layout uses A/B, then X/Y and shoulder buttons in advanced modes.
- **Stick dead zone:** selecting a value applies it to both controllers and the
  second stick. Existing per-stick values are retained until this field changes.
  Timed axes and autofire expose the existing DOSBox-X options.

Controllers can connect after boot. Active controllers retain their slots when
another disconnects; a new connection fills the first vacant slot. Disconnecting
releases buttons, centers axes, and makes the relevant DOS joystick unavailable.
The emulator keeps the selected joystick hardware type stable. Some games only
probe or calibrate during their own setup/startup and must be restarted afterward.
Generic fallback layouts vary between devices; extra modern controls cannot
exceed the selected DOS joystick's capabilities.

The System page configures these shortcuts under `[pidos]`:

| Setting | Default | Action |
| --- | --- | --- |
| `shortcut_reboot` | Ctrl+Alt+Delete | Clean reboot |
| `shortcut_poweroff` | none | Clean poweroff |
| `shortcut_pause` | none | Pause/resume emulation |
| `shortcut_screenshot` | Ctrl+Alt+PrintScreen | Save a PNG to `C:\SYSTEM\CAPTURES` |
| `shortcut_next_cd` | Ctrl+Alt+F4 | Next image in mounted CD image sets |
| `shortcut_next_floppy` | Ctrl+Alt+F5 | Next image in mounted floppy image sets |

Choices are `none`, `Ctrl+Alt+Delete`, `Ctrl+Alt+F1` through `Ctrl+Alt+F10`,
and `Ctrl+Alt+PrintScreen`. F12 remains the emulator's Host key. Use Up/Down or
Tab to reach fields below the visible System rows; the More indicators also
scroll with the mouse. Duplicate assignments are rejected before saving;
hand-edited duplicates are disabled at boot in the table's order.
Invalid hand-edited shortcuts fall back to their defaults and log a diagnostic.
Save and reboot to apply shortcut changes.

Reboot and poweroff use PID 1's existing synchronized shutdown. Pause runs on
key release; repeat the shortcut or press Escape to resume. Disabling a shortcut
passes that combination to the normal DOS input path. Ordinary upstream emulator
shortcuts remain available. No mapper file is loaded or saved for these options.

Screenshots capture the DOS image before host LCD scaling and borders. Names
use the running program and a sequence number, for example `doom_000.png`.
The capture folder is created on writable boots; read-only or missing C:
storage prevents capture without writing elsewhere. Linux's SysRq scancode
for the Print Screen key is accepted too. Holding a shortcut does not repeat it.

Load all discs for a game together, for example:

```dos
MOUNT -U D
IMGMOUNT D C:\IMAGES\DISC1.ISO C:\IMAGES\DISC2.ISO -t iso
```

While playing, Ctrl+Alt+F4 advances to the next disc and wraps to the first.
Use CUE files for CUE/BIN discs. For floppy image sets, release A: first and use
`IMGMOUNT A C:\IMAGES\DISK1.IMG C:\IMAGES\DISK2.IMG -t floppy`, then Ctrl+Alt+F5.
Each action cycles all multi-image drives of its type, keeping drive letters
and notifying the guest of media changes. Physical USB drives, single images,
and C:/Z: are excluded. `IMGSWAP D 2` selects a particular disc at the DOS prompt.
Saved playlists and an in-game disc-selection menu are not implemented.

Host checks cover shortcut dispatch, configuration, image-set selection and
media notifications. Physical Pi capture and in-game disc changes still need
hardware confirmation.

Implementation checks use virtual SDL controllers and mocked system signals.
Physical controller hotplug, game calibration and the Input tab's appearance on
a Pi still need hardware confirmation. Keyboard and mouse hotplug were already
confirmed by the user.

## Network

Network enables IPX-over-UDP multiplayer and NE2000 Ethernet with SLIRP (NAT)
or PCAP (direct wired LAN). Controls include IPX server/client startup, server
address and UDP port, host Ethernet interface, NE2000 I/O/IRQ/MAC, DHCP/static
DOS TCP/IP, TCP/UDP forwarding, and optional native ODI/IPX driver startup.
The image includes the existing NE2000 packet driver and pinned mTCP tools,
manual, license and corresponding source. Networking defaults to disabled.

Windows for Workgroups startup leaves the network card to Windows' own NDIS
and protocol drivers. Set PCAP for real LAN sharing, configure the Windows
installation directory, and run `WFWNET` before `WIN`. Windows media,
Microsoft TCP/IP-32 and native ODI/IPX drivers are user-supplied. WfW sharing
also requires compatible protocols/authentication at the other endpoint.

Save and reboot. `NETUP` retries DOS startup; `NETINFO` reports saved adapter
settings and IPX status. Full setup and limitations are in
[the networking manual](../../docs/networking.txt), installed as
`C:\DOCS\NETWORK.TXT` and `Z:\NETWORK\NETWORK.TXT`.
Generated `C:\SYSTEM\NET` configuration files are refreshed at boot. Manual
startup supports user-managed drivers and mTCP configuration instead.
Physical Pi LAN and Windows file-sharing acceptance remain pending.

## CPU profiles

| ID | Label | cputype | core | cycles |
| --- | --- | --- | --- | --- |
| auto | Automatic | auto | dynamic_rec | auto |
| 286-12 | 286 12 MHz | 286 | normal | fixed 1510 |
| 386-33 | 386DX 33 MHz | 386 | dynamic_rec | fixed 6075 |
| 486-33 | 486DX 33 MHz | 486 | dynamic_rec | fixed 12010 |
| 486-66 | 486DX2 66 MHz | 486 | dynamic_rec | fixed 23880 |
| 486-100 | 486DX4 100 MHz | 486 | dynamic_rec | fixed 33445 |
| pentium-100 | Pentium 100 MHz | pentium | dynamic_rec | fixed 53430 |
| pentium-133 | Pentium 133 MHz (default) | pentium | dynamic_rec | fixed 71240 |
| mmx-200 | Pentium MMX 200 MHz | pentium_mmx | dynamic_rec | fixed 114657 |

MHz values are approximate speed equivalents, using the pinned DOSBox-X speed
presets. Unlike those upstream speed-only presets, these Pi-DOS profiles also
select the CPU model and execution core. They do not change memory or FPU options.
These profiles do not establish application compatibility
or that every supported Pi can sustain the requested budget.

The boot table places Time directly below Date on the left. The right column
lists System, DOS CPU, DOS memory, DOS video, DOS Voodoo, DOS sound and DOS MIDI.
DOS Voodoo displays Enabled/Disabled.
A full-width Regional line below PI-DOS storage and Drives lists keyboard,
country and the active timezone. There is no cycles field. Recognized model/speed combinations show their
profile name without a prefix. Custom budgets show the
configured CPU type without an invented MHz value. `auto` displays as
`Auto (486/Pentium)`. The boot summary and utility share the profile definitions
in `src/common/cpu_profiles.c`.

## Persistence

The existing `C:\SYSTEM\PIDOS.CFG` is the only persistent configuration source.
Only edited keys are updated, including duplicates; unrelated lines are
preserved byte-for-byte. Missing edited keys are appended in their section.
Replacement lines use CRLF. Comments on separate lines are retained; trailing
text on edited assignments is replaced. Mixer percentages live under `[pidos]`
as `mixer_master`, `mixer_sb`, `mixer_fm`, `mixer_gus`, `mixer_cms`,
`mixer_speaker`, `mixer_tandy`, `mixer_tandydac`, `mixer_disney`, `mixer_midi`.
PID 1 emits separate, validated MIXER commands before the user AUTOEXEC.BAT.
This avoids overriding a user's startup commands and the DOS command-tail limit.

The file is reloaded before saving. A complete sibling `PIDOS.TMP` is written
and closed before renaming the previous configuration to `PIDOS.BAK` and moving
the new file into place. A failed replacement attempts to restore the original.
Existing temporary recovery files are never overwritten. Read errors, DOS
read-only attributes, size limits (24 KiB), invalid input and write/rename errors
are reported. Hardware and regional changes apply after reboot; mixer changes can also be
previewed immediately as described above. `REBOOT` or Ctrl+Alt+Delete performs
a full Pi reboot and reloads the saved configuration. Save application data first.

When the current three settings do not match a profile, the interface shows
Custom settings with no profile preselected. Cancel never saves. Missing keys
inherit the packaged defaults. Manual advanced CPU settings can still be edited
with `EDIT`.

## Rebuilding

The compiled utility is tracked so ordinary image builds need no DOS compiler
and no additional network downloads. To rebuild it on Linux x86-64:

```sh
python3 tools/dos/bootstrap.py
make dos-config
python3 tools/dos/test-config.py
python3 tools/image/test-audio.py
python3 tools/image/test-startup.py
python3 tools/image/test-midi.py
make image
```

Bootstrap verifies the SHA-256 of the official Open Watcom
`2026-10-01-Build` archive, pinned in `tools/dos/toolchain.json`, and extracts it
to `output/tools/openwatcom`. The archive is cached under `dl/`. It makes no
system-wide installation. An existing compiler installation can be selected
with `WATCOM=/path/to/watcom make dos-config` (with Linux x64 `binl64` tools).

The build uses `-0 -ms -os` (8086, small model, size optimization) and treats
warnings as errors. The adjacent `PIDOSCFG.build.json` records source hashes,
compiler driver hash and executable hash. Image verification rejects stale
sources or a stale executable and checks the packed initramfs copy.
`bash tools/image/build.sh components` builds only the ARM64 runtime and PID 1
without assembling an SD image. The early native phase-zero helper retains
its minimal build without internal MIDI synthesis.

The Open Watcom runtime license is preserved in `LICENSES/OpenWatcom.txt`;
the image installs it with source information as `C:\DOCS\WATCOM.TXT`.
