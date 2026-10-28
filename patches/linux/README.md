# Linux patches

`0001-usb-floppy-status-polling.patch` caps UFI/CBI USB floppy interrupt-IN
status polling at eight milliseconds. It matches the interface class/subclass/
protocol, without a vendor/product list. Faster intervals and other transports
retain their existing behavior. Full-speed intervals use milliseconds;
high/super-speed intervals use the USB logarithmic microframe encoding.

The cap is applied to the parsed endpoint descriptor before host-controller
configuration, so both the controller and usb-storage use the same interval.
This code is shared by every Pi supported by the ARM64 image, including the
DWC and xHCI controller paths. It does not remove CBI status/error handling,
change SCSI timeouts, or alter DOS write commits or media-change protection.

The kernel emits `Pi-DOS: USB floppy status polling endpoint ... (8ms cap)`
when it shortens an interval. The runtime's endpoint diagnostics then report
the adjusted descriptor. A Pi 5/TEAC FD-05PUB user log confirms the adjusted
8 ms interval and lower control-command latency. Copy speed remained limited
until the runtime also combined file I/O across physical track boundaries;
the user subsequently confirmed improved copying and FORMAT behavior. See
[USB floppy results](../../docs/buildroot-pi5.md#usb-floppy-drive-a).

`br2-external/external.mk` applies the patch on clean builds. The image build
script also applies new patches to existing kernel trees and requests a kernel
rebuild, retaining compiled objects when possible. A patch that no longer
applies causes a clean kernel extraction and build.

Run `python3 tools/image/test-usb-floppy-polling.py` for the focused check of
the patched kernel helper. The image validator checks that the kernel binary
contains the polling-cap diagnostic and records the USB-core source hash.
