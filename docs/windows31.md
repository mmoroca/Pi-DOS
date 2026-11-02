# Windows 3.1 installation and CD performance

Installers can now edit writable `C:\CONFIG.SYS`; supported settings and
compatible driver commands apply on the next boot, before AUTOEXEC.BAT.
See [CONFIG.SYS support](config-sys.md).

Windows installation media must be supplied by the user. These source changes
require a rebuilt Pi-DOS image; physical CD timing and Windows mouse operation
still need confirmation on the Pi.

## Slow installation scan and file reads

Windows Setup searches for an existing installation. Pi-DOS previously sent
physical media-status commands for every directory-search step, even when the
ISO directory was already cached. Successful checks are now reused for up to
100 ms. The search still runs, but repeated entries no longer each require a
USB round trip.

Cooked data reads now use a 64 KiB read-ahead buffer instead of fetching every
2 KiB sector separately. Prefetch stops at track boundaries. If a speculative
read fails, the backend retries only the requested sectors, provided the medium
has not changed. Raw sectors and CD audio bypass this buffer.

Media changes, ejection and disconnect detection clear the buffer. Cached
directory/data access can take up to 100 ms to notice a physical swap; cache
misses and raw/audio reads check the drive immediately. Existing DOS handles
are rejected once the change is detected.

## Mouse stops when Windows starts

The DOS installer uses the built-in INT 33h interface. Windows uses its own
mouse driver, which needs relative motion with the mouse captured. The SDL2
fullscreen surface code previously toggled capture on video-mode changes,
which could release it as Windows loaded. It now explicitly enables capture
for CRT and LCD modes. Startup also enables automatic capture.

On an existing image, try Ctrl+F10 to capture the mouse again. If it still does
not move, exit Windows with the keyboard, run `SETUP` from its installed
directory and check that the mouse is set to Microsoft or IBM PS/2. See the
[DOSBox-X Windows 3.1 guide](https://dosbox-x.com/wiki/Guide%3AInstalling-Windows-3.1x)
for driver choices.

## PATH exceeds 127 characters

The previous system PATH was 128 characters after networking directories were
appended, before Windows added its own directory. It is now 94 characters,
or 105 with `;C:\WINDOWS`, leaving space below Setup's 127-character limit.

`Z:\DEBUG`, `Z:\PATCHING` and `Z:\4DOS` remain available through their full
paths. They are no longer on the default search path.

For an existing image, this prompt command provides the shorter path:

```dos
SET PATH=C:\PROGRAMS\TOOLS;C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\NETWORK;Z:\NETWORK\MTCP;C:\WINDOWS
```

Use your actual Windows directory if different. After installation, ensure
`C:\AUTOEXEC.BAT` includes `SET PATH=%PATH%;C:\WINDOWS` once, since Setup may
have omitted it after the warning. Review existing custom PATH commands before
making this persistent; extra directories still count toward Setup's limit.
