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
managed system-drive backend added by patch 0022. D: uses automatic USB optical
discovery and can be released for CD-ROM images.

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

`0012-automatic-usb-floppy.patch` registers the appliance's internal
`PIFLOPPY.COM` startup command. `src/runtime/usb_floppy_dos.h` reserves `A:`
and delegates FAT12 access to DOSBox-X's FAT driver; `usb_floppy_linux.h`
detects USB floppy interfaces and accesses sectors through Linux SCSI SG_IO.
Discovery is on demand, so hotplug does not require a device-management daemon.
Retired media remain invalid for existing file handles, preventing buffered
writes from reaching a replacement disk. See
[USB floppy support](../../docs/buildroot-pi5.md#usb-floppy-drive-a) for supported
formats and the remaining hardware checks.

`0013-usb-floppy-sector-access.patch` connects the USB floppy to BIOS formatting,
DOS INT 25h/26h and generic track IOCTLs. The adapter accepts standard physical
capacity before a FAT12 boot sector exists and retires old FAT/file caches on
raw writes. BIOS media-type setup handles the call used by FreeDOS FORMAT
instead of falling through to the unsupported-function error. Sector reads
use four read windows and batched READ commands; readiness probes are
coalesced over 100 ms and normal DOS file buffering is retained.

`0014-native-usb-floppy-timing.patch` exempts the physical USB floppy from DOS
file throughput emulation. The adapter handles BIOS read/write/verify requests
directly in track-sized USB transfers, avoiding the upstream per-sector delay
and redundant readiness probes during FORMAT verification. Verification still
reads the device. Opt-in diagnostics record SCSI command durations, gaps and
status in the existing RUNTIME.LOG to investigate hardware pauses.

`0015-usb-floppy-fat-io.patch` skips old-data reads before full-sector overwrites,
unused next-sector reads and clean-buffer rewrites on close/flush for physical
USB floppy files. The adapter also combines repeated FAT updates and adjacent
sector writes within each DOS operation, committing before returning so manual
disk swaps need no unmount. Completed transfers suppress redundant readiness
probes. A reusable ioctl worker lets the emulator service video/timer events
during USB waits, with guest interrupts masked to prevent DOS reentrancy.

The transport now grows sequential file read-ahead to 32 KiB and combines
contiguous DOS-operation writes across physical tracks. It respects Linux's
queue transfer limit and falls back to track/smaller requests if firmware or
SG_IO rejects larger transfers. Short/random reads remain small. Read windows
may overlap after seeks, so writes update every cached copy. Each DOS operation
still commits its writes before returning; BIOS/IOCTL track operations retain
their requested shape. This removes the artificial one-track file-I/O limit
that remained after the kernel's completion-polling cap.

`0016-usb-floppy-metadata.patch` skips directory writes when the encoded entry
is identical, for both root and subdirectories on the physical USB floppy.
Other FAT drives retain their existing behavior. The transport separately
compares staged metadata with bounded snapshots of confirmed on-disk sectors,
trimming unchanged transfer edges without additional reads. Unchanged sectors
between changed neighbours stay in the transfer to avoid extra USB commands.
Snapshots never come from
dirty read windows; raw access, formatting and media retirement clear them.
An entirely unchanged batch still checks media readiness and disk sequence.
Changed metadata commits before the DOS operation returns; file-data and raw
writes are not suppressed. Diagnostics report `unchanged_metadata=N` for
batches that avoid writes. The user subsequently reported performance very
close to Windows across several tests on the Pi 5/Toshiba-branded TEAC drive.
The new trace contains 1,232 SCSI commands without reported errors and no
transfer-size fallback. Exact new timings and per-change speed attribution
remain unmeasured; see the hardware notes in the linked documentation.

`0017-usb-floppy-copy-buffer.patch` increases the built-in shell COPY request
size from 32 KiB to 60 KiB for regular-file writes to the physical USB floppy.
It resolves the opened destination handle's drive, including relative paths
and concatenation; character devices and other destinations retain their
existing request size. This reduces DOS-level FAT/directory commit cycles,
while the transport continues to split USB requests to its supported limit and
commit every DOS write before returning. External XCOPY is unaffected.
Diagnostic logs identify larger-buffer selection with
`Pi-DOS USB floppy COPY: chunk_bytes=61440`. The subsequent Pi 5/TEAC trace
confirms this policy and a COPY write reduction from 31.1 to 28.3 seconds for
the approximately 580,391-byte test file, with FAT/root WRITE commands reduced
from 57 to 33. Read and XCOPY-like phases remain essentially unchanged. All
1,252 captured SCSI commands complete without reported errors or size fallback;
see the linked hardware notes for timing methodology and remaining limits.

`0018-usb-floppy-dpb.patch` handles INT 21h AH=32h and AH=1Fh for the native
USB floppy. The adapter forces a media check and loads the current FAT driver
before delegating DPB serialization to it. Existing valid disks now report
success rather than the upstream removable-drive `0xff` rejection, allowing
FreeDOS FORMAT to choose quick formatting. Blank, invalid and missing media
remain failures; other drives retain their prior behavior. This fixes the
cause of the invalid-BPB warnings rather than suppressing their text.
Diagnostics report `Pi-DOS USB floppy DPB: drive=N valid=0/1`. Host checks
exercise the patched DOS dispatcher and actual FAT DPB serializer; physical
quick-format validation remains pending for this image.

`0019-automatic-usb-cdrom.patch` registers `PICDROM.COM` and reserves D: for a
read-only USB optical backend when enabled by appliance PID 1. The headers in
`src/runtime/usb_cdrom_*.h` discover USB SCSI type-5 devices, issue direct MMC
sector/TOC commands and digitally extract CD audio through the `USBCD` mixer
channel. A worker serializes hardware access and maintains a bounded audio
prefetch buffer. The existing ISO drive gains USB-specific media generations
so disc changes retire caches and old DOS file handles, including equal-length
disc swaps. Image mounts retain their normal media/cache policy. MSCDEX media
queries track changes independently of filesystem accesses. Scope is regular
data CDs, audio CDs and mixed-mode game discs in USB CD/DVD/Blu-ray units;
reading DVD/Blu-ray media is not an acceptance target. See
[USB optical drive D:](../../docs/buildroot-pi5.md#usb-optical-drive-d) for the
host checks and outstanding physical validation.

`0020-list-video-modes.patch` adds `VIDEOMOD.COM` under `Z:\SYSTEM`. It lists
both the active SDL2 display's output modes and the emulated adapter's BIOS/VESA
modes. `/HOST` and `/DOS` filter the output, `/P` pages it and `/?` shows help.
The host list combines duplicate pixel formats and marks the current timing.
The guest list reads the adapter's legacy tables and the existing VESA ROM list,
checking VBE support flags without switching modes. It needs no diagnostics or
host shell. See [command usage](../../docs/buildroot-pi5.md#listing-available-video-modes).

`0021-automatic-usb-storage.patch` registers `PIUSB.COM` and reconciles external
USB filesystem drives at DOS operation boundaries. `src/runtime/usb_storage_*.h`
uses libblkid and synchronous Linux FAT/exFAT mounts, allocating free letters
from E: without persistent identities. Each connection owns a separate mount;
removed media retire open file handles and directory searches before letters
are reused. Checked file writes, metadata flushes and close results reach DOS;
the generic close/flush dispatchers preserve errors instead of reporting success.
The storage I/O worker keeps emulator video/events running during blocking file
transfers and flushes. DOS still waits for completion, with guest interrupts
held to prevent storage reentry; quit/reset drains the pending operation before
releasing its borrowed buffers.
Native builds remain opt-in through `PI_DOS_USB_STORAGE=1` and `PIUSB.COM`.
See [USB storage](../../docs/buildroot-pi5.md#usb-storage-drives-e-onward) for
behavior, host checks and outstanding physical validation.

`0022-managed-system-drive.patch` registers `PISYSTEM.COM` to expose the PI-DOS
volume root as a protected C: drive. PID 1 passes the storage mode through its
host environment; user AUTOEXEC commands do not mount internal storage.
Appliance mode rejects C:/Z: unmounts, Z: relocation, additional host-directory
mounts and SUBST changes. Disk/CD images can use free drive letters, with
explicit unmount required before replacing a removable drive. The shell's
unassigned-drive message points to MOUNT's drive listing. Native development
mounting behavior is unchanged. See [managed drives](../../docs/buildroot-pi5.md#managed-drive-assignments-and-the-c-root).

`0023-classic-shell-names.patch` defaults the supervised shell to uppercase DOS
8.3 aliases in directory listings, prompts, completion and file-operation
feedback. `DIR /-Z` retains an explicit long-name view. File operations and guest
long-filename APIs retain their existing behavior; names on disk are not
rewritten. Native shell display defaults remain unchanged.

`0024-shell-drive-not-ready.patch` checks media readiness before appliance
`DIR`, `LS` and `TREE` pathname expansion. Missing media reports
`Not ready reading drive A`/`D` and returns an error before an empty listing or
summary. Readable media keeps its usual behavior, and native shell commands
are unchanged. Guest DOS media-error reporting stays in the existing backends.

`0025-persistent-volume-labels.patch` adds a checked label setter and routes
LABEL and extended-FCB volume-label create/delete/rename requests through it.
The floppy adapter forwards updates to FAT without constructing a file handle
for FORMAT's label-only request. USB FAT/exFAT updates require no open files,
flush/unmount the volume, run the filesystem label tool with a pinned device
descriptor and exclusive write access, read back its label and remount.
Failures propagate to DOS;
C:/Z: keep their fixed labels and optical labels remain read-only. See
[persistent labels](../../docs/buildroot-pi5.md#persistent-volume-labels).

`0026-fat-volume-label-refresh.patch` reads FAT volume labels from the root
directory when requested, including immediately after FORMAT's raw sector
writes or when reopening a floppy. VOL/DIR no longer depend on an earlier
label search or LABEL command populating an otherwise empty cache.

`0027-native-dos-attributes.patch` enables native FAT/exFAT R/H/S/A metadata
for the managed C: and USB drives. Attribute queries and directory searches read
the same on-disk bits; updates use checked filesystem ioctls and fsync instead
of companion ATR files. Create/open/delete checks enforce read-only files even
when the host process is privileged. FAT-backed floppy/image files preserve
entry types, apply creation attributes, set archive after modifications and
filter hidden/system entries according to DOS search attributes. Internal FAT
path traversal can still enter hidden/system directories. ATTRIB retains its
existing syntax and recursion, with a failing ERRORLEVEL on update failures.
See [DOS file attributes](../../docs/buildroot-pi5.md#dos-file-attributes).

`0028-dos-file-sharing-locks.patch` replaces Linux process-associated sharing
with the DOS-level table in `src/runtime/dos_sharing.h`. It checks open modes,
PSP-owned byte ranges and destructive operations at the DOS file API, including
32-bit lengths, duplicate/inherited grants, FCB/exit cleanup and media identity.
Local stdio and FAT sector/metadata caches remain coherent across independent
opens. Extended create preserves sharing/access flags and cannot truncate after
a sharing failure. DOS name resolution preserves active search results.
See [file sharing and locking](../../docs/buildroot-pi5.md#dos-file-sharing-and-record-locking).
