Pi-DOS Display - Windows 3.1 / 3.11
=================================

Requires Windows 3.1, 3.11 or Windows for Workgroups in 386 Enhanced Mode.
This is a native 16-bit Windows program. It does not need Win32s.

INSTALL
-------
1. Extract the complete PIDOSVID folder onto the Pi-DOS C: drive, preserving
   the S3 and ET4 subdirectories. For example, use C:\DRIVERS\PIDOSVID.
2. Start Windows. In Program Manager choose File, Run, and enter:
     C:\DRIVERS\PIDOSVID\SETUP.EXE
3. Confirm installation and choose the matching video card, resolution,
   colours and system font size. Select Apply.
4. Save your work, exit Windows completely and start it again with WIN.

The installer adds "Pi-DOS Display" to Control Panel. Close and reopen
Control Panel if it was already running. Both drivers are installed;
only the selected card's driver is activated. The source folder can be
removed after successful installation.

CONFIGURE
---------
Open Main, Control Panel, Pi-DOS Display to change the settings later.
Only driver modes whose resolutions are advertised by dac-crt.bin are
offered: currently 640x480, 800x600 and 1024x768.

SF = small fonts (96 dpi); LF = large fonts (120 dpi).
Changes take effect after completely exiting and starting Windows.
There is no live mode switch or automatic timed rollback.

Choose the same video card as PIDOSCFG's Video page. To change the emulated
card, exit Windows, change PIDOSCFG, save and reboot Pi-DOS before installing
or applying that card's driver. This program does not change PIDOSCFG.

ET4000 high-colour modes need DOSBox-X's Sierra RAMDAC support. ET4000
1024x768 offers 16 or 256 colours. Its 24-bit TrueColor mode is omitted
because Pi-DOS's ET4000 emulation implements 15/16-bit high colour.
S3 32-bit modes offer 16.7 million colours and need sufficient video RAM.
The resolution restriction does not set host refresh rate or monitor type.

Existing Windows font files are retained, including localized versions.
Missing fonts are taken from the English driver packages. These packages
are intended for Western Windows; DBCS Windows is not supported.

RECOVERY
--------
Before the first change, the installer saves the original SYSTEM.INI,
WIN.INI and (when present) PROGMAN.INI in WINDOWS\PVIDBACK.
Before each change it saves the previous files in WINDOWS\PVIDLAST.
These are full INI snapshots, including unrelated settings at that time.

If Windows cannot start after a change, from the DOS prompt run:
  CD \WINDOWS
  PVRESTOR.BAT
  WIN
Use your actual Windows directory if different. Run recovery outside Windows.
"Restore previous" in the applet restores the same snapshot.

To recover the very first configuration instead, from DOS copy SYSTEM.INI
and WIN.INI from WINDOWS\PVIDBACK to WINDOWS. Also restore PROGMAN.INI
if it exists in the backup directory.

The installer never overwrites an existing different driver binary. If one
is present, it stops before copying payloads. Use Windows Setup for that
vendor driver installation, or resolve the old files from DOS. Existing
font and vendor INI files are retained. An interrupted payload copy can be
retried; display settings are not changed until Apply.

If updating an earlier Pi-DOS Display applet, exit Windows and copy the new
PIVIDEO.CPL over WINDOWS\SYSTEM\PIVIDEO.CPL from DOS, then start Windows.
The vendor drivers and existing backups can be kept. Existing backup folders
are detected by their directory attributes, including when DOS would report
"access denied" for another attempt to create the same folder.

UNINSTALL
---------
Restore the desired display settings first. Exit Windows and delete
WINDOWS\SYSTEM\PIVIDEO.CPL to remove the Control Panel entry. Vendor files
are kept because Windows may still be using them.

STATUS AND SOURCES
------------------
The package is built from the user's S3 and ET4 driver downloads; these
remain the property of their respective copyright holders. Original OEM
installation manifests and available instructions are included alongside
the expanded payloads. Pi-DOS installer source is in src/winvideo; the
reproducible build is tools/windows/build-video.sh.

The build checks and host-side verification do not establish compatibility
inside Windows on a Raspberry Pi. Validate the package on a copy of your
Windows installation before relying on it.
