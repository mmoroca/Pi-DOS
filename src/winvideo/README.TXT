Pi-DOS Configuration - Windows 3.1 / 3.11
=======================================

Requires Windows 3.1, 3.11 or Windows for Workgroups in 386 Enhanced Mode.
This is a native 16-bit Windows application. It does not need Win32s.

INSTALL / UPDATE
----------------
Extract the complete PIDOSDRV folder to C:\DRIVERS\PIDOSDRV, preserving
its S3, ET4 and AUDIO subdirectories.

The applet is now PIDOS.CPL. Setup runs independently through PIDINST.DLL,
so it can replace an older installed applet without loading its old code.
Keep SETUP.EXE, PIDINST.DLL and PIDOS.CPL together in the extracted folder.

Start Windows and use Program Manager, File, Run to start
C:\DRIVERS\PIDOSDRV\SETUP.EXE. Choose Install / Reinstall. Setup replaces
an existing PIDOS.CPL and removes the previous PIVIDEO.CPL automatically.
If Windows still holds an applet open, close Control Panel and Pi-DOS
Configuration, then click Retry. No manual DOS replacement is required.
Reopen Control Panel and select Pi-DOS Configuration.

The shared main window offers Display, Audio and MIDI, and Pi-DOS settings.
Original audio driver packages are staged below WINDOWS\PIDOSAUD. After
setup completes, the source PIDOSDRV folder is no longer required.

PI-DOS SETTINGS
---------------
The settings page offers CPU, Memory, Video, Sound, Mixer, Input, Network
and System categories. Select a setting and choose Edit or double-click it.
Save writes C:\SYSTEM\PIDOS.CFG using the same presets, validation and
preservation rules as DOS PIDOSCFG. The prior configuration is kept in
C:\SYSTEM\PIDOS.BAK. Close without saving discards pending changes.
Exit Windows and reboot Pi-DOS to apply saved settings.

These settings control emulated hardware and physical audio/display output.
Windows driver pages configure Windows itself. Choose matching Windows
drivers after changing emulated hardware. Mixer settings save boot levels;
they do not preview volume changes during the Windows session.

Keyboard, country and timezone lists use the existing Z:\BIN catalogs.
External USB MIDI discovery remains in DOS PIDOSCFG; this version can retain
or edit the saved destination identity in its MIDI settings page.

DISPLAY
-------
Choose Display to select the Windows video card, resolution, colours and
system fonts. Apply, save work, exit Windows completely, then run WIN again.

Only resolutions advertised by dac-crt.bin and supported by the vendor
packages are offered: 640x480, 800x600 and 1024x768 (34 combinations).
SF means Small Fonts (96 dpi); LF means Large Fonts (120 dpi).
There is no live mode switch or automatic timed rollback.

Choose the same card as Pi-DOS settings / DOS PIDOSCFG. Change the emulated
card, save and reboot Pi-DOS before applying its Windows driver.
ET4000 high colour requires Sierra RAMDAC emulation. Its 1024x768 mode
supports up to 256 colours; its unsupported 24-bit entry is excluded.
S3 32-bit modes offer 16.7 million colours and need sufficient video RAM.
Host refresh rate and CRT/LCD output remain Pi-DOS settings.

Existing localized Windows fonts are kept. Missing fonts come from the
English driver packages. DBCS Windows is outside this package's scope.
Different existing driver binaries are never overwritten during setup.

AUDIO AND MIDI
--------------
The Audio page offers a Windows driver selector and Install selected driver.
Existing Windows 3.1 drivers are included for:

  Sound Blaster 2.0   Creative SBW31, SB20SND + SBFM
  Sound Blaster Pro 2 Creative SBP2W31, SBPSND + SBP2FM + SBPAUX
  Sound Blaster 16    Creative SB16WU, SB16SND + SB16FM + SB16AUX
  Gravis UltraSound  Original GF1 4.11 installer / Windows driver 5.51
  PC speaker         Microsoft SPEAK / SPEAKER.DRV (wave playback)

AdLib uses Windows' own driver: Install selected driver opens Windows Drivers;
choose Add, then Ad Lib. Windows may request its installation disks.
CMS and Tandy show unavailable because no confirmed compatible Windows 3.x
driver package was located. No replacement sound driver is generated.

1. Select the emulated card in Pi-DOS sound settings. Save, exit Windows
   and reboot Pi-DOS before installing its Windows driver.
2. Open Audio and MIDI, select the matching Windows driver and click
   Install selected driver. The Creative and Gravis installers check the
   running BLASTER/ULTRASND resources against the saved configuration.
3. SB2, SB Pro 2, SB16 and PC speaker install within Windows. Existing
   different driver files are never overwritten; if upgrading such files,
   use Windows Drivers / Add / Unlisted with the cached OEM driver folder.
   Restart Windows after installing, then configure the active driver.
4. Gravis uses its original DOS installer, launched from this same page.
   Save work: Windows closes, runs the installer and restarts afterwards.
   Use the displayed ultrasound directory, I/O, IRQ and DMA values. Choose
   Custom Setup to update your Windows directory. Do not install SBOS or
   Mega-Em. Choose an Ultra MIDI map matching the card RAM after restart.

Windows' Timer, MIDI Mapper, [MCI] Sound and [MCI] MIDI Sequencer components
must already be installed. Use the Windows Drivers button to add missing
components using your Windows installation disks.

Native installation preserves existing MIDI maps. After restarting Windows,
use Windows MIDI Mapper to select or create an FM or external MIDI mapping.
Vendor MIDIMAP.CFG files remain in the cached OEM driver folders. The SB16
cache is WINDOWS\PIDOSAUD\SB16WU; others use SB2, PRO2, GUS or SPEAKER.
Do not remove cached folders while their drivers are registered: the sound
VxD uses a path into its card's cache.

Windows INIs are backed up before installation. Native changes are staged
and committed with rollback; unrelated VxDs are preserved. Gravis' DOS helper
also preserves and restores C:\AUTOEXEC.BAT and C:\CONFIG.SYS. If interrupted,
ROOTBACK.TAG and recovery copies remain in WINDOWS\PIDOSAUD\GUS;
PVRUN.EXE prints recovery instructions and refuses another run.

Configure installed audio opens the active wave driver's configuration.
Windows MIDI Mapper opens the mapper's configuration dialog. Windows Drivers
opens the original Windows Drivers Control Panel applet within this app.

RECOVERY
--------
Before the first driver change, original SYSTEM.INI, WIN.INI and (when
present) PROGMAN.INI are saved in WINDOWS\PVIDBACK. Before display changes,
audio installation or driver configuration, previous files are saved in
WINDOWS\PVIDLAST. These are complete INI snapshots, including unrelated
Windows settings at that time. They do not include PIDOS.CFG or MIDIMAP.CFG.

If Windows cannot start after a change, at the DOS prompt run:
  CD \WINDOWS
  PVRESTOR.BAT
  WIN
Use your actual Windows directory if different. Run recovery outside Windows.
Restore Windows INIs in the shared app restores the same last snapshot.
For the first configuration, restore INIs from WINDOWS\PVIDBACK instead.

Existing backup directories are accepted based on their actual attributes,
including when DOS reports access denied for repeated directory creation.

UNINSTALL
---------
Run SETUP.EXE and choose Uninstall, or run SETUP.EXE /UNINSTALL. This removes
both PIDOS.CPL and the legacy PIVIDEO.CPL from WINDOWS\SYSTEM. Close Control
Panel and Pi-DOS Configuration if Setup requests it, then click Retry.
Installed vendor drivers, cached packages, Windows INIs, PIDOS.CFG and
recovery backups are retained, keeping the current drivers operational.
Run Setup again and choose Install / Reinstall to add the controls again.

Replacement is staged as PIDOS.NEW. Previous applets are renamed to
PIDOS.OLD and PIVIDEO.OLD until the new applet is installed. On failure,
Setup restores their previous names. If recovery itself fails, exit Windows
and restore those .OLD files to their matching .CPL names in WINDOWS\SYSTEM.
Setup preserves existing .OLD files rather than discarding recovery data.

BUILD AND VALIDATION
--------------------
Installer source: src/winvideo. Build: tools/windows/build-video.sh.
The S3/ET4 payloads are user supplied. Public audio archives are pinned in
configs/windows-audio.json and cached locally; vendor binaries are not
added to the repository. They remain their respective owners' property.

The prior display applet and directory fix were user-confirmed on Pi-DOS.
The new shared Windows UI and real audio installation still need Pi
validation. Host checks cover the shared settings engine, package hashes,
EDID filtering, distinct installer/applet module names, migration from the
legacy applet, reinstall/uninstall and configuration rollback. The actual DOS
handoff was tested with a harmless substitute installer, confirming that
startup files are preserved and Windows-side changes are retained.
