# Pi-DOS 1.0 User's Guide

A DOS-compatible computer for Raspberry Pi

Edition 1 · 30 October 2026  
Technical baseline: ff0c194

<!-- FRONTMATTER -->

## Welcome

Welcome to Pi-DOS. With a Raspberry Pi, a keyboard and a display, you can use a computer that starts at a familiar DOS prompt. You can organize files, run compatible DOS programs, connect removable media and choose the kind of PC hardware your software expects.

Pi-DOS combines a dedicated Raspberry Pi system with an emulated x86 computer. DOS programs see PC processors, memory, video adapters and sound devices. The Raspberry Pi provides the real display connections, USB ports, storage and audio outputs. You normally work entirely from DOS; there is no desktop or Linux login to pass through.

You do not need to understand the underlying system to begin. This guide starts with connecting your equipment and reaching the prompt, then introduces files, programs and setup. Later chapters cover sound, removable disks, networking and troubleshooting.

### About this guide

This edition describes Pi-DOS 1.0 at implementation baseline `ff0c194`. Existing installations may retain earlier settings. When this guide says *default*, it means the fresh-image setting at that baseline, unless stated otherwise.

This is a user guide rather than a record of development. It explains implemented behavior, but does not certify every Raspberry Pi, peripheral, game or Windows installation. Appendix E summarizes the hardware checks recorded in the technical reference and identifies features that still need physical confirmation.

The supplied technical reference is the authority for Pi-DOS-specific behavior. The organization and restrained print design take inspiration from the 1993 Microsoft MS-DOS 6 Concise User's Guide. The explanations and procedures here are written for Pi-DOS; an MS-DOS command or utility is not assumed to exist just because an older manual describes it.

### Finding the information you need

If you are new to DOS, read Chapters 1 through 3 in order. If you already know DOS, read Chapter 1 and the Pi-DOS differences in Chapter 4, then use the hardware and media chapters as needed.

| To learn about | Read |
| --- | --- |
| Connecting equipment, preparing a card and first startup | Chapter 1 |
| Commands, paths and a short DOS tutorial | Chapter 2 |
| Copying files, running programs and making backups | Chapter 3 |
| Setup, saved configuration and user startup | Chapter 4 |
| Processor speed and memory | Chapter 5 |
| CRT and LCD displays | Chapter 6 |
| Sound cards, physical audio and MIDI | Chapter 7 |
| USB storage, floppies, CDs, images and ZIP drives | Chapter 8 |
| Keyboard, mouse, gamepads and shortcuts | Chapter 9 |
| Clock, regional settings and networking | Chapter 10 |
| Benchmarks, demos and screenshots | Chapter 11 |
| Recovery, troubleshooting and updates | Chapters 12 and 13 |
| Commands, defaults and technical background | Appendices A through E |

### Conventions used in this guide

A *drive* is a storage location identified by a letter, such as `C:`. A *directory*, also called a folder, holds files and other directories. A *path* identifies a file or directory by its location.

Text in `monospaced type` represents commands, filenames, paths, configuration entries or messages. Bold text emphasizes controls and important choices. A triangle marks the start of a procedure. A note gives additional information; an important notice calls attention to a consequence such as data loss.

When a procedure says to type a command, type the command and press **Enter**. Do not type a displayed prompt such as `C:\>`. Commands on separate lines are entered separately. Example files and directories are illustrative; substitute your own names where appropriate.

**Ctrl+Alt+Delete** means hold Ctrl and Alt, press Delete, then release the keys. On Pi-DOS this normally requests a clean reboot of the whole Raspberry Pi. It is not a way to return safely from a game with unsaved progress.

### Before you begin

Have a copy of the Pi-DOS image intended for your board, suitable storage and a way to write the image from another computer. Keep that other computer available for copying files or repairing a card that Pi-DOS cannot mount.

Pi-DOS supplies a DOS-compatible environment, not an original Microsoft DOS installation. Games, commercial applications, Windows, MIDI ROMs and some sound assets may need to be supplied separately. Use software and assets you are entitled to use, and retain their accompanying instructions.

<!-- MAINMATTER -->

## Chapter 1 Getting Started

This chapter helps you prepare the equipment, start Pi-DOS and complete its welcome screens. Before changing an existing card, make a backup of files you want to keep.

### What you need

Use a supported 64-bit Raspberry Pi, an appropriate power supply, an SD card or supported boot arrangement, an HDMI display or suitable adapter, and a wired USB keyboard. A USB mouse is useful for some applications and for setup. A wired Ethernet connection is optional; normal DOS use does not require Internet access.

| Raspberry Pi family | Image targets |
| --- | --- |
| Pi 2 | Model B revision 1.2 only |
| Pi 3 | 3B, 3B+ and 3A+ |
| Pi Zero | Zero 2 W only |
| Pi 4 | 4B and 400 |
| Pi 5 | 5, 500 and 500+ |
| Compute Modules | Corresponding 64-bit variants with suitable carrier and USB host hardware |

The original Pi Zero, Zero W and Pi 2 revision 1.1 are excluded. A target in this list is not a promise that every carrier, peripheral and display combination has been tested. Basic boot and input are recorded for Pi 3 and Pi 5, and Pi 4 with corrected matching firmware.

Wi-Fi and Bluetooth are not configured by this image. Use wired USB input and, when needed, wired Ethernet. Zero and Compute Module systems need appropriate USB host adapters or carrier hardware.

### Preparing the system card

The development image starts at about 769 MiB. Its data partition is designed to expand into available card space during startup. Choose a card comfortably larger than the image and large enough for your programs, disk images and backups.

#### To write a new Pi-DOS card

1. On another computer, obtain the intended `pi-dos-arm64-sdcard.img` image, or its diagnostic variant.
2. Insert the destination card and open your image-writing utility.
3. Select the Pi-DOS image and carefully select the destination device.
4. Write and, if your utility offers it, verify the image.
5. Eject the card using that computer's normal removal procedure.

> **Important** Writing an image replaces the destination card's existing partition layout and data. Confirm the device before writing. This is a preparation task on another computer, not a DOS FORMAT operation.

A freshly written card contains a `BOOT` partition and a FAT32 partition labelled `PI-DOS`. Do not rename the PI-DOS partition: Pi-DOS uses that label to find its persistent files. The DOS label displayed for C: is SYSTEM; this is intentional and is different from the host partition label.

### Connecting your equipment

#### To connect and start Pi-DOS

1. With the Raspberry Pi off, insert the prepared system card.
2. Connect the display and USB keyboard. Connect a mouse if you intend to use one.
3. Connect any USB audio or external MIDI device you intend to select at startup.
4. Connect wired Ethernet if you want network clock synchronization or networking.
5. Turn on the display and apply power to the Raspberry Pi.

On a dual-HDMI board, default HDMI audio uses HDMI0, the connector nearest the USB-C power connector. Use that connection when relying on the default audio route.

Pi-DOS may check and expand its data partition before reaching DOS. Allow it to finish; do not remove power during a storage operation. Expansion is restricted to the expected Pi-DOS layout. Unexpected layouts are left unchanged rather than formatted.

The startup table reports the physical board and selected emulated PC hardware. Its CPU, DOS memory, video and sound entries describe the configured environment. They do not mean that those components exist as physical PC expansion cards.

### Completing the welcome screens

Fresh images normally run a two-stage setup. The initial defaults are a Spanish keyboard, Spain as the DOS country, the Europe/Madrid timezone and a CRT monitor profile. Set these to match your equipment and location.

#### To complete first-boot setup

1. In the welcome screen, select your keyboard layout, DOS country and timezone.
2. Choose CRT or LCD. For an LCD, select a fixed output resolution supported by your display.
3. Press **F2** to save. The startup welcome requests a clean reboot to apply these choices.
4. After reboot, use the typing test to check your keyboard and review the local date and time.
5. Press **F2** or **Esc** to finish the review and continue.

On the initial welcome screen, **Esc** skips setup, discards pending edits and continues with saved defaults. You can reopen the welcome later:

```dos
PIDOSCFG /FIRSTBOOT
```

When you launch it manually, saving regional settings still requires you to run `REBOOT` explicitly. A save that fails does not complete the pending setup stage.

Time may be incorrect before network synchronization finishes. Pi-DOS does not hold up DOS startup while waiting for a connection. Offline time depends on an available working real-time clock. Chapter 10 explains manual clock changes and synchronization.

### Your first command prompt

Normal startup ends at:

```text
C:\>
```

Type the following to see the files and directories on C:

```dos
DIR
```

To identify the running product, type:

```dos
VER
```

If you see a Z: recovery prompt instead, the persistent data partition could not be used. Do not assume your files have been erased. See Chapter 12 before changing the card.

### Turning off or restarting

#### To turn off Pi-DOS

1. Save your work in the running program.
2. Exit the program to the DOS prompt.
3. Type `POWEROFF` and allow the shutdown sequence to finish before disconnecting power.

For a clean restart, type:

```dos
REBOOT
```

The default **Ctrl+Alt+Delete** shortcut also requests a clean whole-machine reboot, including while a program is running. Save first: flushing storage cannot save an application's unsaved document or game state.

A Pi 5 short power-button press is implemented as another clean shutdown request, though the technical reference records physical acceptance of that button path as pending. Use the command as the primary documented procedure. A long hold forces power off and can interrupt writes.

## Chapter 2 Learning DOS Basics

At the command prompt, you tell Pi-DOS what to do by typing commands. Most daily tasks involve selecting a drive, changing directories and naming the files you want to use.

This tutorial uses a new directory called GUIDE on C:. If you already have a directory with that name, substitute a different unused name.

### Understanding the prompt

In `C:\>`, C: is the selected drive and the backslash is its root directory. When you change to a subdirectory, the prompt normally shows that location. DOS commands and paths are generally case-insensitive.

You can use uppercase or lowercase when entering the commands in this chapter. Pi-DOS normally displays uppercase short names even when the underlying file has a longer name.

### Viewing a directory

#### To see the contents of the current directory

1. At the prompt, type `DIR`.
2. Read the displayed file and directory names.
3. For a long listing, use the paged form below.

```dos
DIR /P
```

For a compact, wider listing, use:

```dos
DIR /W
```

A directory listing shows what is available at the selected location. An empty or disconnected removable drive can report *drive not ready*. That message does not mean the drive contains an empty disk.

### Creating and changing directories

#### To create a practice directory

1. Select C: by typing `C:`.
2. Return to the root by typing `CD \`.
3. Create the directory and enter it:

```dos
MD GUIDE
CD GUIDE
```

`MD` makes a directory. `CD` changes the current directory. A subdirectory is simply a directory inside another directory.

#### To create and enter a subdirectory

1. While in `C:\GUIDE`, type `MD NOTES`.
2. Type `CD NOTES`.
3. Type `CD ..` to return to GUIDE.
4. Type `CD \` to return directly to the root of C:.

The two periods in `CD ..` mean the parent directory. A single backslash at the start of a path means the root of that drive.

### Creating a small text file

Pi-DOS includes an editor selected by the command `EDIT`. Its writable personalized copy normally lives in `C:\PROGRAMS\TOOLS`.

#### To create a practice note

1. Type the command below.
2. Enter a short sentence in the editor.
3. Use the editor's save and exit controls to save the file and return to DOS.

```dos
EDIT C:\GUIDE\HELLO.TXT
```

Now display the file:

```dos
TYPE C:\GUIDE\HELLO.TXT
```

Use the editor's own help for its editing keys. The selected editor is PEDIT Light, so do not assume that every key sequence from Microsoft's MS-DOS Editor applies.

### Copying and renaming a file

#### To copy your note into NOTES

1. Type `CD \GUIDE`.
2. Type the following command:

```dos
COPY HELLO.TXT NOTES\HELLO.TXT
```

The first path names the source. The second names the destination. COPY leaves the source in place. If a destination file already exists, check the command's overwrite prompt before proceeding.

#### To rename the copied note

1. Type `CD NOTES`.
2. Type `REN HELLO.TXT FIRST.TXT`.
3. Type `DIR` to see the new name.

Renaming changes a name; it does not create a second copy. If the rename is refused, check whether a program still has the file open or whether a file with the new name already exists.

### Understanding paths

| Example | Meaning |
| --- | --- |
| `FIRST.TXT` | A file in the current directory |
| `NOTES\FIRST.TXT` | A file in NOTES beneath the current directory |
| `\GUIDE\NOTES\FIRST.TXT` | A file reached from the root of the current drive |
| `C:\GUIDE\NOTES\FIRST.TXT` | A complete path on C: |
| `..` | The parent of the current directory |

A full path is useful in instructions and batch files because it makes the intended location clear. When working with removable drives, verify the assigned letter before copying or deleting files.

### Selecting another drive

To select an available drive, enter its letter followed by a colon:

```dos
E:
```

A: normally represents a USB floppy, D: a USB optical drive, and E: onward eligible USB data volumes or explicit image/ZIP mounts. C: and Z: are reserved. Selecting an automatic removable letter can succeed even if its media is not ready.

Use the following command to inspect current mappings:

```dos
MOUNT
```

### Filenames and extensions

Classic DOS names have up to eight characters before a period and up to three after it. The extension commonly indicates a file's role: TXT for text, BAT for batch commands, and COM or EXE for executable programs.

Pi-DOS uses uppercase 8.3 aliases by default. It retains underlying long names and long-filename interfaces, but applications may choose their own presentation. To request the long-name directory view, type:

```dos
DIR /-Z
```

To return to the short-name view, type `DIR /Z`. Use the short name shown by DIR when an older application cannot handle the long name.

### Using wildcards

An asterisk matches a group of characters; a question mark matches one character in classic DOS matching. For example, `DIR *.TXT` lists text files in the current directory. Use a listing first to check a wildcard before applying it to a command that changes files.

> **Important** A broad pattern such as `*.*` can affect many files. This guide uses explicit filenames for destructive examples so the intended target is visible.

### Deleting the practice files

#### To remove only the tutorial files

1. Confirm that your original note is still in `C:\GUIDE`.
2. Delete the copied note, then remove its now-empty directory:

```dos
DEL C:\GUIDE\NOTES\FIRST.TXT
RD C:\GUIDE\NOTES
```

3. If you also want to remove the original note and practice directory, type:

```dos
DEL C:\GUIDE\HELLO.TXT
CD \
RD C:\GUIDE
```

`DEL` deletes a file. `RD` removes an empty directory. Treat deletion as permanent unless you have a backup; this guide does not promise a bundled undelete tool.

## Chapter 3 Managing Files and Programs

C: is your normal persistent workspace. Use it for applications, documents and configuration. You can add files by connecting supported USB storage or by opening the PI-DOS partition on another computer after a clean shutdown.

### Knowing where files belong

| Directory | Normal use |
| --- | --- |
| `C:\GAMES` | Games and their data |
| `C:\PROGRAMS` | Applications and user tools |
| `C:\DOCS` | Documentation and personal text files |
| `C:\DEMOS` | Demoscene productions and menu content |
| `C:\TEMP` | Temporary files |
| `C:\SYSTEM` | Pi-DOS configuration, launchers and managed files |
| `C:\SYSTEM\CAPTURES` | PNG screenshots |

Keep your own applications in named subdirectories. Avoid replacing files in SYSTEM just to install an unrelated program. Appendix B provides a fuller directory map.

### Adding files from another computer

#### To copy files directly to the system card

1. Save your work and run `POWEROFF`.
2. After shutdown, remove the card and connect it to another computer.
3. Open the FAT32 partition labelled PI-DOS.
4. Copy your files into appropriate directories such as GAMES or DOCS.
5. Eject the card safely, reinstall it and start Pi-DOS.

The root of PI-DOS becomes the root of C:. Do not add an extra `c` directory around all your files. Leave the BOOT files and PI-DOS volume label intact.

### Copying from USB storage

Eligible FAT or exFAT USB volumes receive free letters starting at E:. Letters can change after a reboot or when devices are removed, so inspect the mapping each time.

#### To copy a document from USB

1. Connect the USB device and type `MOUNT`.
2. Confirm its assigned letter and inspect its contents with `DIR E:`; substitute the actual letter.
3. Copy the intended file, for example:

```dos
COPY E:\LETTER.TXT C:\DOCS\LETTER.TXT
```

Wait for successful completion before removing the device. Close files in editors or applications first. You can optionally release an idle USB drive with `MOUNT -U E`; that connection remains ejected until it is physically reconnected.

COPY is a file-copy command, not a whole-directory backup recipe. Do not assume a command copying `*.*` includes every nested directory or hidden file. Use the file manager or a suitable backup tool and verify the result.

### Using Doszip Commander

Type `DZ` to open the supplied two-panel file manager. Use its displayed controls and help to browse, copy and organize files. Its settings live beside the program under `C:\PROGRAMS\DOSZIP`.

Before copying or deleting, check the source panel, destination panel and selected files. A two-panel view helps compare locations, but it does not prevent overwriting a newer file with an older one.

### Running a program

#### To start an installed DOS program

1. Select the drive containing the program.
2. Change to the program's directory.
3. Type the program's executable name.

For example, if you have supplied a compatible program named APP.EXE:

```dos
C:
CD \PROGRAMS\MYAPP
APP
```

Follow the program's own installation instructions. Some software expects to run from its installation directory; other software needs a particular CPU, memory, video adapter or sound setup. Chapters 5 through 7 explain those choices.

Pi-DOS runs compatible COM, MZ EXE and BAT files. A modern Windows executable is not a DOS program. Windows 3.x is a separate, user-supplied installation path, not an application automatically included with Pi-DOS.

### Editing text and keeping backups

Use `EDIT` for plain-text files such as your startup batch file. Before a significant edit, copy the existing file to a distinct backup name. For example:

```dos
COPY C:\AUTOEXEC.BAT C:\AUTOEXEC.OLD
EDIT C:\AUTOEXEC.BAT
```

Choose a backup name that does not overwrite a copy you still need. Setup has its own one-generation configuration backup, but it is not a backup of your documents or games.

### File attributes and open files

DOS attributes identify read-only, hidden, system and archive-marked files. Inspect a file with:

```dos
ATTRIB C:\DOCS\LETTER.TXT
```

To mark that file read-only, type `ATTRIB +R C:\DOCS\LETTER.TXT`. To allow normal modification again, type `ATTRIB -R C:\DOCS\LETTER.TXT`.

New writable opens, replacement and deletion of read-only files are rejected. A program that already holds a writable handle retains that access. Hidden and system files may be absent from an ordinary listing; `DIR /A` requests attribute-aware listing.

Pi-DOS also enforces DOS file sharing and record locking. If a copy, rename or deletion reports a sharing or lock violation, close the program using the file before retrying. Do not disable sharing as the first response to a legitimate conflict.

### Backing up your work

#### To make a complete user-data backup

1. Save all applications and run `POWEROFF`.
2. Open the PI-DOS partition on another computer.
3. Copy its complete contents, including hidden files and directories, to a separate backup location.
4. Verify that important documents, game saves and SYSTEM configuration files are present.
5. If you use ZIP drives, verify that each archive's matching `.PFS` directory was copied too.

Keep an independent backup before updates, migration or filesystem repair. Copy BOOT as well, or retain the matching original image, when you need to preserve the full boot setup. A backup stored only on the same card does not protect against card failure.

### Checking space and names

Use directory listings to review storage and remove only files you recognize. Do not treat ordinary USB data mappings as raw DOS hard disks: FDISK, raw disk formatting and DOS defragmenters are not the maintenance route for those mappings.

FAT32 has a per-file size limit below 4 GiB. Keep that in mind when storing large images on C:. USB exFAT is a supported data-volume format, but an older DOS application may have smaller file-size or free-space limits of its own.

## Chapter 4 Configuring Your System

Pi-DOS separates the computer's saved hardware settings from the commands you want to run at startup. Use PIDOSCFG for supported setup controls, PIDOS.CFG for advanced settings, and AUTOEXEC.BAT for your own startup commands.

### Opening setup

Type:

```dos
PIDOSCFG
```

Setup has eight pages: CPU, Memory, Video, Sound, Mixer, Input, Network and System. The System page scrolls to additional fields. Follow its arrow or More indicators to reach them.

| Key | Operation |
| --- | --- |
| F5 / F6 | Previous / next page |
| Tab / Shift+Tab | Next / previous control |
| Enter | Edit or activate the focused control |
| Arrow keys | Navigate choices |
| Home / End, PgUp / PgDn | Move within lists |
| F2 or Alt+S | Save edited settings across all pages |
| F3 or Alt+R | Restore defaults on the current page, pending Save |
| Esc | Cancel without saving ordinary settings |

Some lists support searching by typing. Mouse selection is optional. F3 changes the current page only and does not take effect on disk until you save.

#### To change a setting

1. Run `PIDOSCFG` and move to the relevant page.
2. Select the control and choose the new value.
3. Review any related settings on other pages.
4. Press **F2** to save.
5. Return to the prompt and run `REBOOT` to apply the saved configuration.

Hardware, display, input, network and regional changes normally require rebooting. The live mixer is the documented exception: eligible channels preview immediately. Chapter 7 explains when mixer changes are live and how Cancel restores them.

### Understanding PIDOS.CFG

The authoritative user configuration is:

```text
C:\SYSTEM\PIDOS.CFG
```

The file contains INI-style sections. Pi-DOS-specific settings appear in `[pidos]`; other sections configure the DOS runtime. On each boot, Pi-DOS reads the saved file and generates the configuration it needs internally.

Existing settings are preserved. A new image or newly added default does not necessarily replace a choice saved on an older installation. Use the current setup controls or restore defaults on the relevant page when you deliberately want the current defaults.

#### To edit an advanced setting

1. Make an independent copy of PIDOS.CFG before editing.
2. Run `EDIT C:\SYSTEM\PIDOS.CFG`.
3. Change only settings whose effects you understand.
4. Save and exit, then run `REBOOT`.

Prefer setup for its supported controls. A valid-looking upstream runtime option may conflict with Pi-DOS's managed hardware and drive policies. Selecting a managed monitor profile, for example, makes that profile authoritative over low-level display choices.

### Setup backups and interrupted saves

When setup saves, it writes a complete PIDOS.TMP first. It then keeps the previous configuration as PIDOS.BAK and installs the new PIDOS.CFG. Each successful save replaces the previous backup; this is a single generation, not a history.

Setup preserves unrelated settings and separate-line comments while updating edited keys. It reports read-only files, invalid values, configuration files over 24 KiB and storage failures. It does not overwrite an existing temporary recovery file.

If a save fails or power was interrupted, inspect PIDOS.CFG, PIDOS.BAK and PIDOS.TMP before deleting or renaming anything. Use another computer if C: is not writable. Chapter 12 explains recovery.

### Customizing user startup

Your startup file is:

```text
C:\AUTOEXEC.BAT
```

Pi-DOS runs it after standard system initialization and first-boot setup. Add personal environment variables, PATH extensions or commands here. Do not duplicate the system's automatic drive registration or hardware initialization.

#### To add a tools directory to PATH

1. Create your tools directory and place compatible executables in it.
2. Back up AUTOEXEC.BAT.
3. Add a line such as the following, then save:

```dos
SET PATH=%PATH%;C:\PROGRAMS\MYTOOLS
```

4. Reboot and check the resulting PATH at the prompt.

The normal PATH already includes the writable editor/tools directory, SYSTEM and several Z: utility directories. Replacing it with only your new directory can make standard commands harder to find.

TEMP and TMP normally point to `C:\TEMP` on writable boots. Leave a writable temporary location available for setup's live-control queries and applications that need working files.

### CONFIG.SYS and familiar MS-DOS advice

Pi-DOS uses its internal DOS environment and native startup system. Copying a PC's CONFIG.SYS does not replace the Pi's boot process. Driver loading and CONFIG.SYS compatibility are limited to what the runtime implements.

Do not copy memory-manager, disk-compression or startup recipes from MS-DOS 6 without checking that they apply. This guide does not prescribe HIMEM, EMM386, DoubleSpace or MS-DOS system utilities as Pi-DOS requirements.

A separately booted DOS image has its own startup rules and needs separate validation. The instructions in this guide assume the standard Pi-DOS environment unless stated otherwise.

## Chapter 5 Choosing Processor Speed and Memory

The CPU and Memory pages describe the emulated PC, not the physical Raspberry Pi. Changing a CPU profile changes the processor model and execution speed presented to DOS software; it does not change the Pi's physical clock.

### Selecting a CPU profile

Before the first DOS session, a fresh image detects the Raspberry Pi and selects a CPU preset: **486DX2 66 MHz on Pi 3**, **Pentium 75 MHz on Pi 4**, or **Pentium 120 MHz on Pi 5**. This also applies if you skip the welcome setup. Pi 2, Zero 2 and Compute Module 3 use the Pi 3 preset; Pi 400 and Compute Module 4/4S use Pi 4; Pi 500 and Compute Module 5 use Pi 5. Unknown models keep the packaged Pentium 120 fallback.

The CPU page marks all three board defaults and adds **[recommended]** to the detected board's preset. **Restore defaults** selects that preset; Save and reboot apply it. Updates and moving an existing card to another board preserve the saved CPU settings.

These are starting points, and the conservative Pi 3 preset still needs Pi-DOS hardware performance confirmation. Lower the profile if a program has audio dropouts or slow response. The MHz labels indicate approximate performance classes. They do not guarantee the speed, timing or benchmark score of a particular original PC.

| Profile ID | Displayed class |
| --- | --- |
| `auto` | Automatic |
| `286-12` | 286 12 MHz |
| `386-33` | 386DX 33 MHz |
| `486-33` | 486DX 33 MHz |
| `486-66` | 486DX2 66 MHz, Pi 3 default |
| `486-100` | 486DX4 100 MHz |
| `pentium-75` | Pentium 75 MHz, Pi 4 default |
| `pentium-100` | Pentium 100 MHz |
| `pentium-120` | Pentium 120 MHz, Pi 5 default |
| `mmx-166` | Pentium MMX 166 MHz |

#### To try a slower PC profile

1. Open `PIDOSCFG` and select CPU.
2. Choose a fixed profile appropriate to the program, such as 486DX2 66 MHz.
3. Save and run `REBOOT`.
4. Start the program again and compare its behavior.

You can list profile choices from the command line with `PIDOSCFG /LIST`. For example, `PIDOSCFG /PROFILE 486-66` selects that saved profile; reboot afterward. Profile selection does not independently change memory or FPU settings.

Use one change at a time when diagnosing timing problems. A slow menu, fast animation or music problem can depend on application timing rather than raw processor throughput.

### Fixed speed and Automatic

Fixed profiles request a particular execution budget. Automatic adapts execution speed, which can help some workloads but change timing behavior in others. A Pi still needs enough sustained performance to meet the requested budget.

The technical reference records a MIDI rhythm or stuttering issue in tested Sierra software, including Leisure Suit Larry 6, with Automatic. Tested fixed profiles avoided it. Use a fixed profile for that case; disabling presentation VSync did not solve it and introduced tearing.

These are global settings. Pi-DOS does not supply per-game hardware profiles or automatic game-specific tuning. Record a working configuration for programs that need special settings.

### Choosing DOS memory

The Memory page offers 1, 2, 4, 8, 16, 32, 64 and 128 MiB. The default is **16 MiB**. This is separate from the physical RAM reported for the Raspberry Pi.

#### To change the memory size

1. Open the Memory page in PIDOSCFG.
2. Choose a size supported by the selected CPU.
3. Save and reboot.
4. Run the application and check its own requirements and messages.

More memory is not always better. Some older programs fail with more than 16 MiB; others require a smaller configuration. Setup rejects sizes outside the selected CPU's address range.

### Conventional and extended memory

DOS software may refer to conventional memory, upper memory, HMA, XMS, EMS, DPMI or a DOS extender. These are different ways programs use memory and services in the emulated PC.

Pi-DOS inherits these facilities from its DOS engine. Availability depends on configuration and the program. A message about insufficient conventional memory is not necessarily solved by increasing the total emulated RAM.

Start with the program's own installation notes. Avoid adding PC memory-manager drivers merely because an older guide recommends them for MS-DOS. An application may need its supplied DOS extender, and Pi-DOS does not provide every licensed extender or Windows component.

### Checking compatibility methodically

Use the default configuration first unless the program specifies otherwise. If it fails, note the message, try the CPU family and memory size it expects, then check video and sound requirements.

Self-modifying code, copy protection, unusual drivers and timing-sensitive demos can need individual testing. Pi-DOS aims for compatibility; it does not promise cycle-accurate reproduction of every PC.

## Chapter 6 Setting Up Your Display

There are two separate display choices: the video adapter seen by DOS software and the monitor profile used to present its image on your physical screen. Choose both to suit your equipment and application.

### Choosing an emulated video adapter

The default adapter is **S3 Trio64**. The Video page also offers CGA, Hercules, Tandy, EGA, VGA, Tseng ET4000 and VESA VBE 2.0/3.0 choices.

An application may require a particular adapter or mode. Select the adapter before starting that application. Choosing an older adapter can help reproduce its intended graphics, but may make newer high-resolution modes unavailable.

Software Voodoo emulation is enabled by default with maximum memory. It emulates 3dfx hardware in software; it does not mean the Pi contains a Voodoo card or that every accelerated game will be fast. Host Glide passthrough is disabled in the baseline.

### Using a CRT monitor

CRT is the default monitor profile. It follows the guest's rendered dimensions and attempts to select a corresponding host mode near the guest refresh rate. Standard VGA text begins at 720 by 400 pixels at roughly 70 Hz.

Use a display and adapter that can accept the selected timing. A listed mode is not proof that a physical monitor will lock onto it. Pi-DOS supplies VGA-era display timings, not original Hercules electrical output.

CRT mode does not add artificial scanlines, phosphor effects or shadow masks. Its intended appearance comes from a physical CRT. It also avoids software aspect correction; physical monitor geometry determines the final shape.

### Using a flat-panel display

LCD holds a selected host resolution while DOS changes video modes. Available choices are 640x480, 800x600, 832x624, 1024x768, 1280x720 and 1920x1080. The retained LCD default is 1024x768.

#### To configure an LCD

1. In PIDOSCFG, select the LCD monitor profile.
2. Choose a resolution your display accepts.
3. Select **4:3** or **Original pixels** for the image shape.
4. Leave linear filtering off for sharper pixel edges, or enable it for a smoother scaled image.
5. Save and reboot.

LCD mode fits the image with centered borders and reduces oversized guest modes to fit. Original pixels preserves the rendered raster ratio: for example, 320x200 is 8:5. A 4:3 choice deliberately changes that displayed proportion.

There is no automatic native-panel detection, integer-scale control or user refresh selector in the described setup. LCD preferences remain saved while CRT is selected.

### Inspecting available modes

Use:

```dos
VIDEOMOD /P
VIDEOMOD /HOST
VIDEOMOD /DOS
```

`/P` pages the output. `/HOST` lists advertised physical display modes; `/DOS` lists DOS BIOS/VESA modes. `VIDEOMOD /?` shows help.

Pi-DOS enables presentation VSync. That controls presentation timing and is separate from the engine's emulated VGA VSync setting. A guest/host refresh mismatch may still cause repeated or dropped frames.

### If the display is blank or incorrectly shaped

Check power, cable, connector and the display's supported modes first. If the saved CRT profile selects a timing your display cannot accept, review the saved monitor settings from another computer or use a compatible display to select LCD.

For more detail, enable diagnostics and inspect VIDEO.LOG after the next boot. It records requested guest dimensions and refresh, the rendered dimensions and the selected host mode. Chapter 12 describes collecting logs before they are replaced.

## Chapter 7 Setting Up Sound and Music

A game's sound setup and Pi-DOS's physical audio output are different choices. The game addresses an emulated PC sound device; Pi-DOS mixes that sound and sends it to HDMI, the analogue jack or a USB DAC.

### Choosing a PC sound device

The default is **Sound Blaster Pro 2**, with base address **220h**, **IRQ 7** and **DMA 1**. The PC speaker is independently enabled.

Other presets include None, Tandy, AdLib, Creative Game Blaster/CMS, Sound Blaster 2.0, Sound Blaster 16 and Gravis Ultrasound Classic. Choosing a preset enables that managed device and disables competing managed cards. MIDI remains independent.

#### To match a game's digital sound setup

1. In PIDOSCFG, choose the sound device and resources you want.
2. Save and reboot.
3. Run the game's sound setup.
4. Select the same device, address, IRQ and DMA values.
5. Run the game's sound test if provided, then save its settings.

At the DOS prompt, `SET BLASTER` shows the current sound environment. Do not select SB16 in a game while Pi-DOS is still configured for SB Pro 2 merely because the game offers that option.

GUS software may need user-supplied compatible patch files under the configured `C:\ULTRASND\MIDI` location. Those patches are not bundled as a general guarantee.

### Choosing the physical output

The System page selects HDMI, 3.5 mm jack or external USB DAC. Save and reboot after changing it.

| Output | What to check |
| --- | --- |
| HDMI | Default; HDMI0 on dual-HDMI boards |
| 3.5 mm jack | A board with the socket and enabled analogue support |
| USB DAC | A compatible playback-capable USB audio-class device connected before boot |

Pi-DOS waits briefly for USB audio discovery. If the selected output is missing, that boot remains silent; it does not silently choose another output. With multiple USB DACs, selection follows ALSA card/device order rather than a user-selected stable device identity.

All internally mixed audio, internal MIDI and extracted CD audio follows the selected output. External MIDI module audio is separate.

### Choosing a MIDI profile

| Profile | Required sound source |
| --- | --- |
| None | MIDI disabled |
| Roland SC-55 | `C:\SYSTEM\MIDI\SC55.SF2` |
| Roland GM | `C:\SYSTEM\MIDI\GMROLAND.SF2` |
| Roland MT-32 | Matching control and PCM ROMs in `C:\SYSTEM\MIDI\MT32` |
| Roland CM-32L | Matching ROMs in `C:\SYSTEM\MIDI\CM32L` |
| External USB MIDI | A connected, selected MIDI interface/module |

SC-55 is the fresh-image profile, but it needs the corresponding SoundFont. The SC-55 and Roland GM choices use SoundFont synthesis; they are not verified hardware-accurate emulations of those sound modules. MT-32 and CM-32L use Munt with their respective ROM pairs.

Assets must be supplied through the project's authorized asset workflow and match its manifest. They are not included in a fresh source clone. Missing assets or a missing device are reported in the boot MIDI row and disable that managed MIDI profile for the boot.

Choose matching music hardware in the game's own setup. Sound Blaster digital effects and internal MIDI music can play together.

### Connecting an external MIDI module

#### To select an external MIDI destination

1. Connect the USB MIDI interface and turn on the module before boot.
2. In PIDOSCFG, choose External USB MIDI and select the destination.
3. Use **F5** in the destination picker if you need to rescan.
4. Save and reboot.
5. In the game, select the music hardware expected by your module.

Pi-DOS saves the destination by stable identity. For devices without a unique serial number, keep the interface on the same USB port. A missing or ambiguous destination is not replaced by another module.

Connect the module's audio outputs to its own speakers or mixer. MIDI carries performance messages, not the module's resulting audio. The internal MIDI mixer control cannot change the volume of an external module's analogue output.

### Adjusting the mixer

The Mixer page provides master and device-specific levels. Values range from 0 to 200 percent for left and right channels. Levels above 100 percent can clip.

#### To preview and save mixer levels

1. Start with the intended sound hardware already active.
2. Open PIDOSCFG and select Mixer.
3. Adjust an enabled channel marked **LIVE**.
4. Press **F2** to keep the preview and save it, or cancel to restore the starting levels.

Pending hardware changes suspend live preview and restore original levels. Inactive channels or unavailable live control show **AFTER REBOOT**. Temporary queries need writable C: storage.

If music stutters with Automatic CPU, try a fixed profile as described in Chapter 5. If there is no sound at all, check the physical output, game resources, mixer levels and MIDI assets separately.

## Chapter 8 Using Disks and Removable Media

Pi-DOS manages its main drive letters and discovers supported removable devices. Physical devices, disk images and ZIP drives have different rules; choose the right method for the media you are using.

### Understanding drive assignments

| Letter | Normal assignment |
| --- | --- |
| A: | Automatic USB floppy |
| C: | Persistent system/data partition, DOS label SYSTEM |
| D: | Automatic USB optical drive |
| E: onward | Eligible USB data volumes or free-letter mounts |
| Z: | Internal runtime utilities, DOS label PI-DOS |

C: and Z: are protected. You cannot replace or relocate them in appliance mode, even when C: is unavailable. Arbitrary host-directory mounts, ordinary directory overlays and SUBST mappings are disabled.

Type `MOUNT` to list mappings. Free a removable mapping before assigning that letter to an image. Automatic USB discovery skips letters already occupied by an explicit mount.

### Using USB sticks and external disks

Pi-DOS automatically recognizes eligible FAT12, FAT16, FAT32 and exFAT USB data volumes. It supports ordinary USB sticks, hard disks, SSDs and card readers, including appropriate MBR and GPT partitions.

Volumes receive the lowest free letter starting at E:. Connected volumes retain their letters, but assignments are not saved across reboot. The system's own disk is excluded from USB data discovery.

#### To use a USB data volume

1. Connect the device and allow a moment for discovery.
2. Type `MOUNT` and identify its drive letter.
3. Use DIR and ordinary file commands on that letter.
4. Close files and wait for successful writes before removal.

For optional ejection, type `MOUNT -U E`, substituting the correct letter. Close DOS files first. Reconnect the physical device before using that ejected connection again.

USB data mapping provides file access, not a raw emulated DOS hard disk. Pi-DOS does not automatically repair, repartition or format these volumes. Unsupported, hidden, reserved, conflicting or already mounted partitions can be skipped.

### Using a USB floppy drive

A: automatically discovers suitable USB floppy hardware. Standard supported capacities span 320 KiB through 2.88 MiB with 512-byte sectors and valid FAT12 for file access; the drive must support the actual disk capacity.

#### To copy a file from floppy

1. Connect the drive and insert the disk.
2. Type `DIR A:` to inspect it.
3. Copy the file, for example:

```dos
COPY A:\LETTER.TXT C:\DOCS\LETTER.TXT
```

You can attach a drive before or after boot. After a disk change, Pi-DOS resets that drive's current directory and invalidates old handles. Close files before changing disks. Wait for command completion and the drive light to go out before removal.

If write-protect status cannot be reported by the drive, Pi-DOS uses it read-only. A USB stick is not automatically treated as a floppy drive.

### Formatting a floppy

> **Important** FORMAT destroys the selected floppy's existing contents. Confirm A: and the inserted disk. Do not use these instructions on a disk containing the only copy of your files.

#### To quick-format an already recognizable floppy

1. Verify that the disk is expendable and not write-protected.
2. Type `FORMAT A: /Q`.
3. Read and answer FORMAT's prompts, then wait for completion.

A full format can be requested with `FORMAT A: /U`. Quick formatting needs a recognizable existing format. Full formatting fills and verifies addressable sectors and writes FAT12 at the existing capacity; it cannot create missing physical tracks, change recording density or reproduce copy-protected formats.

### Using physical CDs

D: automatically represents the first eligible USB optical drive. The integration targets ordinary data, digital-audio and mixed-mode CDs. DVD/Blu-ray drive hardware may be recognized, but DVD/Blu-ray disc reading and burning are outside this integration's scope.

#### To use a data CD

1. Connect the drive and insert the disc.
2. Type `DIR D:`.
3. Run the disc's DOS installer or copy permitted files according to its instructions.

No manual mount or reboot is needed for a physical disc swap. Close files first. An absent or empty drive reports not ready. Only one physical optical drive receives the automatic D: mapping.

CD audio is extracted digitally and mixed through the selected physical audio output. It does not use a drive's analogue cable. Physical mixed-mode, CD-audio and hotplug acceptance are still pending in the technical baseline; try your actual drive and discs before depending on them.

### Mounting a CD image

Store your image files in a directory such as `C:\IMAGES`. Create that directory if needed and copy the files there from another computer or removable media.

#### To use an ISO image on D:

1. Close programs using D:.
2. Release the automatic optical mapping.
3. Mount the intended image:

```dos
MOUNT -U D
IMGMOUNT D C:\IMAGES\GAME.ISO -t iso
```

The image must exist at that path. For mixed-mode CUE/BIN sets, use the CUE description with its associated BIN files, following the pinned engine's supported formats.

To restore automatic physical optical discovery, release the image mapping and run `Z:\SYSTEM\PICDROM.COM` with D: free, or reboot. This restores the automatic backend; it does not convert an image into physical media.

### Using more than one image

#### To load a two-disc CD set

1. Place both images in the intended directory.
2. Release D: and mount the full set together:

```dos
MOUNT -U D
IMGMOUNT D C:\IMAGES\DISC1.ISO C:\IMAGES\DISC2.ISO -t iso
IMGSWAP D 2
```

`IMGSWAP D 2` selects the second image. The default **Ctrl+Alt+F4** shortcut advances and wraps mounted multi-image CD sets. It affects all such CD image drives, not physical discs or single-image mounts.

For floppy image sets, release A: and mount the image set with `-t floppy`. The default **Ctrl+Alt+F5** shortcut cycles multi-image floppy sets. Image geometry and format must be supported by the engine.

There are no saved disc playlists, automatic disc selection or in-game selection menu. Keep a note or appropriate startup commands for the image sets you use. Physical in-game switching remains a validation item.

### Mounting a ZIP as a writable drive

ZIPMOUNT presents an archive as a DOS drive without changing the ZIP itself. Writes go into a sibling directory with the same basename and the extension `.PFS`.

#### To run a program from a ZIP drive

1. Choose a free drive letter; check MOUNT first. In this example E: must be free.
2. Mount your archive and switch to that letter:

```dos
ZIPMOUNT E C:\GAMES\DOOM.ZIP
E:
```

3. Start the program as required by the archive's contents.
4. Exit it and switch away from E: before unmounting:

```dos
C:
ZIPMOUNT E -u
```

The example assumes you supplied the archive and that its contents are suitable. ZIPMOUNT is not an automatic game launcher or installer.

### Keeping ZIP saves

For `DOOM.ZIP`, changes normally live in `DOOM.PFS` beside it. Newly written files and modified copies take precedence over the archive. Deletion information is also retained so deleted archive entries remain hidden after remounting.

Copy **both the ZIP and its matching PFS directory** when backing up or moving the program. Do not remove either while mounted. To reset to the archive's original content, unmount first and remove the overlay only after backing up any saves you want to keep.

A read-only mount uses `-ro`:

```dos
ZIPMOUNT E C:\GAMES\DOOM.ZIP -ro
```

Read-only prevents new writes but still shows existing overlay changes. It does not mean a pristine view of the original ZIP. Failed overlay creation fails the mount; saves are not redirected to temporary RAM.

### Labels and media safety

Floppy and USB labels can be changed using LABEL, provided files are closed and the volume is writable. For example:

```dos
LABEL E:USB GAMES
VOL E:
```

Labels are limited to 11 DOS bytes, uppercased using the guest code page. C: SYSTEM and Z: PI-DOS labels are protected; optical labels are read-only. Renaming the PI-DOS host partition from another computer can prevent startup from finding C:.

Successful writes are flushed before completion, but unplugging during a write can still lose data. Use clean shutdown for the system card and close files before ejecting removable devices.

## Chapter 9 Using Input Devices and Shortcuts

Pi-DOS accepts supported wired USB keyboards, mice and controllers through the Raspberry Pi's input system. DOS applications then receive the emulated keyboard, mouse or gameport interfaces they expect.

### Keyboard and mouse

Choose the keyboard layout in setup independently of your DOS country and timezone. If letters or punctuation are wrong, change the keyboard layout first; country controls number/date conventions rather than the physical arrangement of keys.

A DOS application needs support for the emulated mouse interface to use a mouse. A connected physical mouse does not give every text program a mouse-driven interface. Keyboard and mouse hotplug are recorded as working in the supplied reference, while application-specific behavior can vary.

### Choosing a joystick or gamepad mode

The default presents two conventional two-axis, two-button DOS joysticks. Other choices include four-axis configurations, Thrustmaster FCS and CH Flightstick modes. Select a mode the game understands.

Known controllers use SDL mappings in Automatic layout. Unknown devices can fall back to raw axes and buttons; Raw bypasses normalization. You can choose left stick with D-pad, stick only or D-pad only, swap the first two buttons, and set a dead zone.

#### To configure a controller

1. Connect the controller and open PIDOSCFG's Input page.
2. Choose the DOS joystick type and layout appropriate to the game.
3. Adjust the dead zone or button swap if needed.
4. Save and reboot.
5. Start the game and use its joystick calibration routine.

Controllers can connect and reconnect after boot. Active devices keep their slots; a new device fills the first vacancy. Disconnection releases buttons and centers axes. A game that probes only at startup may need restarting after a reconnect.

Modern extra buttons are limited by the selected DOS joystick interface. There is no gamepad-to-keyboard mapper or per-game controller profile system. Physical calibration and gamepad hotplug still need acceptance for your particular hardware and game.

### System shortcut keys

| Default shortcut | Action |
| --- | --- |
| Ctrl+Alt+Delete | Clean reboot of the Raspberry Pi |
| Ctrl+Alt+PrintScreen | Save a numbered PNG screenshot |
| Ctrl+Alt+F4 | Next image in mounted CD image sets |
| Ctrl+Alt+F5 | Next image in mounted floppy image sets |
| None assigned | Poweroff |
| None assigned | Pause/resume |

F12 remains the runtime Host key. The listed Pi-DOS shortcuts can be reassigned to None, Ctrl+Alt+Delete, Ctrl+Alt+F1 through F10, or Ctrl+Alt+PrintScreen. Setup rejects duplicate assignments.

#### To change a system shortcut

1. Open the System page and find the desired action; scroll if needed.
2. Choose an unused key combination or None.
3. Save and reboot.
4. Check the assignment before using it in an application.

A disabled shortcut returns to normal DOS input. Assigned combinations suppress repeated key events. Shift/GUI variants remain available to the guest.

If you assign Pause, press and release the shortcut to pause. Repeat it, or press Escape, to resume. Pausing is not a save state and does not write a game's progress to disk.

### Taking a screenshot

Press **Ctrl+Alt+PrintScreen** with the default assignment. Pi-DOS saves numbered PNG files in `C:\SYSTEM\CAPTURES`, using a name based on the program, such as `doom_000.png`.

The capture contains the DOS image before LCD scaling and borders. C: must be available and writable. If it is not, the screenshot is not silently stored elsewhere. Physical Pi capture acceptance remains open in the baseline, although dispatch and image-generation checks are recorded.

Pi-DOS does not supply save states, rewind or integrated video-recording workflows. Use an application's own save function for progress and documents.

## Chapter 10 Setting the Clock, Region and Network

Your keyboard, DOS country and timezone are independent settings. Networking also has two layers: the Raspberry Pi's background connection and the optional network adapter seen by DOS software.

### Changing regional settings

#### To select your local conventions

1. Open the System page in PIDOSCFG.
2. Select the keyboard layout matching your keyboard.
3. Select the DOS country and, when appropriate, code-page choice.
4. Select a timezone from the installed catalog.
5. Save and reboot.

The baseline defaults are keyboard `sp`, country `34` and timezone `Europe/Madrid`. Searchable lists help locate other choices. Timezone rules provide daylight-saving changes; receiving network time does not choose your timezone for you.

### Reviewing date and time

Use DATE and TIME to inspect or change the DOS clock. Follow their prompts if you choose to make a manual change.

Manual changes can suspend host-to-DOS time synchronization. To resume synchronization, type:

```dos
DATE /S
```

The background host service obtains time over wired networking when it can. Startup does not wait for that service, and the displayed time may change after synchronization. Offline startup depends on an available working real-time clock.

### Host networking and guest networking

| Layer | Purpose |
| --- | --- |
| Raspberry Pi host connection | Wired DHCP, DNS and network time |
| Optional DOS connection | Networking used by DOS or Windows software |

The host can synchronize time even while DOS IPX and NE2000 networking are disabled. Both guest features are off by default. The appliance image has no Wi-Fi, Bluetooth, IPv6, SSH server or normal Linux login workflow.

You can use DOS offline. The current image still starts its host network services in the background, but unavailable networking does not prevent normal DOS startup.

### Enabling DOS TCP/IP with SLIRP

SLIRP provides NAT-style guest connectivity through an emulated NE2000 adapter. It is the simpler starting point for DOS TCP/IP when direct attachment to the physical LAN is unnecessary.

#### To configure the bundled DOS TCP/IP tools

1. In PIDOSCFG's Network page, enable NE2000.
2. Choose SLIRP and the intended DOS TCP/IP startup mode.
3. Keep adapter resources consistent with the packet driver; the defaults are I/O 300h and IRQ 10.
4. Choose DHCP or appropriate static guest settings.
5. Save and reboot, then run `NETINFO` to inspect settings and status.

If startup did not complete, use `NETUP` to retry. Retry is bounded so an absent lease or cable does not block DOS indefinitely.

The bundled mTCP tools include FTP, HTGET, TELNET, PING and DNSTEST. Their documentation, license and corresponding source are under `Z:\NETWORK\MTCP`. Use that manual for each tool's arguments.

Default SLIRP guest addressing is 10.0.2.15 with mask 255.255.255.0, gateway 10.0.2.2 and DNS 10.0.2.3. These are guest addresses, separate from the Pi's physical LAN address. Inbound services through NAT need appropriate TCP/UDP forwarding configuration.

### Preserving custom TCP/IP settings

Pi-DOS generates networking files under `C:\SYSTEM\NET` each boot. `MTCPCFG` normally points to the generated MTCP.CFG there; DHCP updates its lease data.

Do not treat edits to generated files as persistent application configuration. For custom mTCP options, select Manual startup and maintain a separate user configuration referenced by your own MTCPCFG setting.

The full installed networking guide is available at:

```text
C:\DOCS\NETWORK.TXT
Z:\NETWORK\NETWORK.TXT
```

### Playing through an IPX tunnel

IPX tunnel mode connects compatible emulator endpoints over UDP. One system acts as server and the others connect as clients. The default UDP port is 213.

#### To prepare an IPX tunnel session

1. Enable IPX in the Network page.
2. Choose server mode on one system and client mode on the others.
3. Set the server address and matching port as required.
4. Save, reboot and check the connection using NETINFO and the installed networking guide.
5. Start the games with compatible multiplayer settings.

NETUP retries saved startup. IPXNET provides manual control and PING facilities. Firewalls must allow the chosen UDP traffic. A tunnel does not put native IPX frames onto a real retro-PC LAN.

### Connecting directly to a wired LAN

PCAP presents the guest's Ethernet frames on a selected wired host interface, usually `eth0`. The guest needs its own MAC/IP configuration, and the network must permit the additional MAC address. This is not Wi-Fi bridging.

For native IPX peers, use PCAP with the IPX tunnel disabled and supply compatible ODI components, including LSL.COM, ODI NE2000.COM and IPXODI.COM. The ODI driver is different from the bundled TCP/IP packet driver. Match I/O, IRQ and frame type. Some applications also need a compatible NETBIOS driver.

SLIRP is not a substitute for raw Ethernet protocols such as native IPX or NetBEUI. Follow NETWORK.TXT for the direct-LAN configuration rather than mixing drivers from different stacks.

### Windows for Workgroups

Windows for Workgroups 3.11 is user-supplied. Its PCAP path uses its own NE2000 NDIS/protocol stack and may require a separately supplied Microsoft TCP/IP-32 package.

Match adapter resources, select Windows startup in PIDOSCFG, and run **WFWNET before WIN**. The helper loads the supplied installation's IFSHLP.SYS through DEVICE.COM and runs NET START. Do not first load DOS packet or ODI stacks for this path.

A separately booted DOS installation instead uses its own CONFIG.SYS and AUTOEXEC.BAT. Direct LAN access does not add SMB2/3 or modern authentication to Windows for Workgroups. Physical LAN, native-IPX and WfW sharing acceptance remain open in this baseline.

## Chapter 11 Using the Supplied Tools

The bundled menus provide convenient starting points for benchmarks and demos. They preserve your selected hardware settings, so the menu itself does not make every item compatible with every configuration.

### Editing and file management

`EDIT` starts the writable personalized PEDIT Light editor. `DZ` starts Doszip Commander. Their settings are stored with their writable program copies under PROGRAMS.

ARJ 2.41 is available at `Z:\BIN\ARJ.EXE`. Consult its included help and notices for archive operations. ZIPMOUNT, described in Chapter 8, mounts a ZIP as a DOS filesystem; it serves a different purpose from extracting an archive with an archiver.

No managed BASIC, Pascal, C or assembly development suite is installed as a product feature. You may supply compatible DOS development tools. The Open Watcom toolchain used to build setup is a development-host tool, not a bundled DOS IDE.

### Running benchmarks

Type `BENCH` to open the benchmark menu. Press **Q** to return to the caller's drive and directory.

| Menu choice | Benchmark |
| --- | --- |
| 1 | Superscape 3DBench 1.0c |
| 2 / 3 | Chris's 3D Benchmark, VGA / VESA |
| 4 / 5 | PC Player, VGA / VESA |
| 6 / 7 | Doom 1.9 shareware DEMO3, minimum / maximum detail |
| 8 / 9 / A | Quake 1.06 shareware DEMO1, three video resolutions |

Doom and Quake benchmark sound is disabled. Quake needs an FPU, and high-resolution choices need a suitable VESA adapter. At the end of a Quake run, read the console FPS and use QUIT to return.

#### To compare two configurations

1. Record the Pi model, Pi-DOS version, CPU profile, RAM, video adapter and monitor settings.
2. Run the same benchmark option and record the result.
3. Change one setting, save and reboot.
4. Repeat the same option under otherwise comparable conditions.

A score measures the configured emulated environment. A CPU profile label does not guarantee an original PC's score. Cooling, throttling and the Pi's ability to sustain the requested emulation speed can affect results.

### Running demos

Type `DEMOS` to open the numbered menu. Press **H** for setup notes or **Q** to return.

The pack contains Second Reality, Unreal, Panic, Crystal Dream I, Crystal Dream II, Stars: Wonders of the World, Caero, Inside and Fishtro. Inside opens its original setup first. The menu does not change CPU, memory, video or sound settings.

Start with the default 16 MiB and fixed CPU profile, then follow each production's original notes. Match SB/SB Pro resources to Pi-DOS. Tracker music uses the selected sound card, not the separate SC-55 MIDI profile.

Some productions require EMS or extended memory. If timing is wrong, try an appropriate fixed 486 profile. Archive and launch-file checks do not establish physical playback, music synchronization or speed for your hardware.

### Finding documentation and notices

Look in C:\DOCS, the application's directory and relevant Z: tool directories. Benchmark and demo directories retain original material and provenance files. Doszip retains its matching source archive and GPL notice.

An image update does not necessarily add newly introduced data packs to an existing C:. See Chapter 13 for updating user-data payloads without replacing unrelated SYSTEM files.

## Chapter 12 Diagnosing and Solving Problems

Begin with the smallest useful observation: the exact message, whether C: is available, what changed since the last successful boot, and whether the problem affects one program or the whole system.

### When the system reaches Z: instead of C:

A Z: recovery prompt means Pi-DOS could not use the expected data storage. There is no invented temporary C: hiding the problem. Common causes include a missing or unusable partition, an incorrect label, an older ext4 layout or filesystem errors.

#### To inspect the card safely

1. Avoid repeated configuration saves or other write attempts.
2. Use a clean shutdown path where available. The default supervised reboot shortcut can operate without writable C:; the disk-based REBOOT batch path needs its request file.
3. With the Pi off, connect the card to another computer.
4. Check for the expected BOOT and FAT32 PI-DOS partitions.
5. Back up readable data before changing labels or running that computer's filesystem-repair tools.

Pi-DOS does not provide a general filesystem-repair tool or a Linux recovery shell. Do not reflash the card as the first response if it contains unbacked-up files.

### When C: is read-only

A failed writable mount can fall back to read-only C:. Reading programs may still work, but setup, saves, logs and screenshots cannot persist. Pi-DOS does not redirect those writes to RAM.

Preserve readable files from another computer and investigate the card's filesystem, capacity and health. A read-only attribute on one file is different from an entirely read-only drive; ATTRIB cannot repair a read-only volume mount.

### When setup cannot save

Check whether C: is writable, PIDOS.CFG is read-only, the configuration exceeds 24 KiB, or a recovery PIDOS.TMP remains. Read the reported error before modifying files.

#### To recover an earlier configuration

1. Shut down and open PI-DOS on another computer if necessary.
2. Preserve copies of PIDOS.CFG, PIDOS.BAK and PIDOS.TMP wherever present.
3. Inspect their contents and identify a known-good complete configuration.
4. Restore that copy as `SYSTEM/PIDOS.CFG` without discarding your preserved originals.
5. Eject the card safely and restart.

The BAK file is the immediately previous successful configuration, not necessarily a factory default. Do not blindly delete TMP: it may contain the complete new file from an interrupted replacement.

### When the screen is black

Check display power and the physical connection. A CRT timing that the panel or adapter cannot accept can leave you with no usable picture. Try a known-compatible display or review the saved monitor configuration from another computer.

The current baseline includes a fix for a static black first-boot welcome after display initialization, but physical confirmation of that exact correction is pending. If the symptom persists, retain the version information and diagnostics rather than repeatedly rewriting data.

### When there is no sound

Check these items separately:

- The selected physical output exists and is connected; HDMI audio defaults to HDMI0.
- The game selects the same sound card, address, IRQ and DMA as Pi-DOS.
- The relevant mixer channel is enabled and has an audible level.
- A MIDI profile has its required assets or external destination.
- External MIDI module audio is connected to speakers or a mixer.

An unavailable selected jack or USB output does not trigger an automatic HDMI fallback. Missing MIDI assets can disable that profile even while digital sound effects work.

If Sierra MIDI timing is wrong with Automatic CPU, select a tested fixed profile. Keep presentation VSync enabled; disabling it did not resolve the recorded problem.

### When a removable drive is not ready

For A: or D:, confirm that compatible hardware is connected and media is inserted. For an image, confirm it was mounted on the letter you are using. For USB data, use MOUNT to check whether the assigned letter changed.

Close and reopen files after media changes. Pi-DOS deliberately invalidates stale handles so an application cannot write to a replacement disk using old state. Unsupported filesystems or reserved partitions may not appear as data drives.

A floppy may be read-only because of its write-protect tab or because the drive cannot report protection status. A physical optical disc is read-only by design.

### When files appear to be missing

Check the current directory, drive letter and long/short-name view. Use `DIR /A` when hidden or system attributes may be involved. USB letters are not persistent across reboot.

For a ZIP drive, confirm that the archive and matching PFS directory are both present. Existing overlay deletions remain effective even on a read-only mount. Do not delete the overlay to investigate until you have backed it up and unmounted it.

### When a program reports a sharing or lock error

Error 32 is a sharing violation; error 33 is a lock violation; error 36 indicates sharing-buffer overflow. Close other DOS programs or handles using the object before retrying. Open files and directories containing them cannot be deleted or renamed freely.

Locks coordinate DOS owners inside this runtime instance. They do not provide cross-machine SMB server locking. A historical error-33 built-in loader issue was corrected; persistent failures on the current build need the program name and exact operation recorded.

### When a game runs badly

Return to a known configuration and change one item at a time. Try a fixed CPU profile, the memory size the game expects, the intended video adapter and matching sound resources.

More memory or a faster profile can make older software less compatible. An unrecognized executable may be a modern Windows program rather than DOS software. Copy-protected media, unusual drivers and timing-sensitive demos need individual validation.

Use the original application's manual alongside this guide. Pi-DOS cannot guarantee every old installer, Windows system or copy-protection scheme.

### When the clock or network is wrong

A correct timezone does not establish a network connection, and a correct host connection does not enable DOS networking. Review the boot regional values and NETINFO independently.

Check wired Ethernet and the chosen guest backend. Use NETUP to retry guest startup. Use `DATE /S` to resume clock synchronization after manual DOS changes. For PCAP, verify the physical interface and the network's allowance for the guest's additional MAC.

### Collecting diagnostic logs

Diagnostics are off by default. Enable Diagnostic logs in setup and reboot, or use the following saved configuration setting:

```ini
[pidos]
diagnostics=true
```

| File | Information |
| --- | --- |
| `C:\SYSTEM\BOOT.LOG` | Boot, storage and lifecycle |
| `C:\SYSTEM\RUNTIME.LOG` | Runtime and backend errors |
| `C:\SYSTEM\NETWORK.LOG` | Host DHCP, DNS and time services |
| `C:\SYSTEM\VIDEO.LOG` | Display mode selection and presentation |

#### To preserve a useful problem report

1. Enable diagnostics and reboot.
2. Reproduce the problem once and note the steps.
3. Copy the relevant logs before another reboot replaces them.
4. Include Pi model, Pi-DOS version, selected profiles, connected devices and the exact message.
5. Keep the saved configuration with the report if it helps explain the settings.

With logging disabled, old log files remain untouched and can be stale. Missing or read-only C: prevents persistent logs. Serial diagnostics remain available for developer investigation, but there is no general Linux login console in normal use.

## Chapter 13 Updating and Maintaining Pi-DOS

Back up data and configuration before an update. A compatible update can preserve C:, but changes to boot assets, defaults and data packs do not all follow the same path.

### Updating a compatible installation

For an already compatible FAT32 volume-root installation, many runtime updates can replace BOOT's `Image` while retaining PI-DOS data. A particular update may also need matching firmware, device trees, overlays or command-line files.

#### To prepare for an update

1. Record the current Pi-DOS version and important setup choices.
2. Shut down and back up the complete PI-DOS partition.
3. Retain the matching BOOT files or a copy of the old image.
4. Follow the update's instructions for the exact matching boot assets.
5. Eject the card safely, start it and check your files and settings.

Do not mix firmware components merely because their filenames match. The recorded Pi 4 USB correction depends on a matched firmware pair. Some Pi 5 D0 boot paths need the appropriate DTB and `bcm2712d0.dtbo` overlay.

### Preserving existing settings

Startup normally seeds missing files and preserves customizations. An existing installation can therefore retain earlier CPU, sound or monitor defaults. Select a current preset or restore defaults on the intended setup page when you deliberately want to adopt current behavior.

Replacing Image does not automatically install every new benchmark, demo or data payload into an existing C:. Follow the pack's own merge instructions and preserve unrelated SYSTEM files.

### Moving from an older layout

An older ext4 image needs a data backup, a newly written FAT32 image and restoration of user files. Replacing only the kernel does not convert its filesystem.

Older `c/` and `d/` wrapper directories are not migrated automatically. Move their contents to suitable locations in the new PI-DOS volume root. An old FAT32 data volume labelled DOSDATA must be relabelled PI-DOS for current detection, after confirming its layout and backing it up.

Do not restore an entire obsolete SYSTEM directory over a new installation without reviewing the configuration and launchers it contains. Preserve your old copy and merge intentional customizations.

### Adding the demo pack

The project's `make demos` workflow produces `output/pi-dos-demos.zip`. To add that pack to an existing card, merge its DEMOS directory and SYSTEM\DEMOS.BAT into the PI-DOS root while retaining other SYSTEM files.

This archive is produced on a development computer; it is not generated by typing make at the Pi-DOS prompt. Appendix D summarizes image-building boundaries.

### Maintenance habits

Save documents and games before rebooting. Keep independent backups, especially of ZIP overlays. Wait for writes to complete before removing media. Retain the source image and matching boot assets used for a working configuration.

When a fault appears after an update, compare the version, saved configuration and matching assets before trying unrelated hardware settings. A short record of what changed is often more useful than repeated reinstallations.

<!-- APPENDICES -->

## Appendix A Command Quick Reference

This appendix lists commands used in this guide. It is not a complete inventory of the inherited DOS engine. Syntax examples assume the standard Pi-DOS shell and actual files at the illustrated paths.

### Everyday DOS commands

| Command | Purpose / example |
| --- | --- |
| `DIR` | List the current directory |
| `DIR /P` | Page a long listing |
| `DIR /W` | Use a compact wide listing |
| `DIR /A` | Include attribute-aware entries |
| `DIR /-Z` / `DIR /Z` | Long-name / short-name view |
| `CD \DOCS` | Change directory from the drive root |
| `CD ..` | Go to the parent directory |
| `MD WORK` | Make a directory |
| `RD WORK` | Remove an empty directory |
| `COPY source destination` | Copy a named file |
| `REN oldname newname` | Rename a file |
| `DEL filename` | Delete a file; check the target first |
| `TYPE filename` | Display a text file |
| `EDIT filename` | Edit a text file |
| `ATTRIB filename` | Inspect DOS file attributes |
| `ATTRIB +R filename` | Mark a file read-only |
| `VOL E:` | Display volume information |
| `LABEL E:USB GAMES` | Set a writable removable volume label |
| `SET BLASTER` | Show the current sound environment |
| `VER` / `VER /R` | Pi-DOS version / extended runtime information |
| `DATE`, `TIME` | Inspect or manually set the DOS clock |
| `DATE /S` | Resume host clock synchronization |

### Pi-DOS setup and lifecycle

| Command | Purpose |
| --- | --- |
| `PIDOSCFG` | Open the setup utility |
| `PIDOSCFG /LIST` | List CPU profiles |
| `PIDOSCFG /PROFILE 486-66` | Select the fixed 486DX2 profile; reboot afterward |
| `PIDOSCFG /FIRSTBOOT` | Open welcome/pending clock review |
| `PIDOSCFG /HELP` | Show setup command-line help |
| `PIDOSCFG /CONFIG C:\SYSTEM\TEST.CFG` | Use an explicitly named configuration file |
| `REBOOT` | Request clean whole-Pi reboot through the disk request path |
| `POWEROFF` | Request clean shutdown |
| `EXIT` | At the top-level shell, also ends the runtime for shutdown |

An alternate configuration file is useful for advanced inspection or preparation; normal boot still reads the authoritative PIDOS.CFG. Do not expect saving TEST.CFG to change the next startup by itself.

### Media and video commands

| Command | Purpose |
| --- | --- |
| `MOUNT` | List current drive mappings |
| `MOUNT -U D` | Release the removable D: mapping |
| `IMGMOUNT D C:\IMAGES\GAME.ISO -t iso` | Mount an ISO on a free D: |
| `IMGSWAP D 2` | Select image 2 from the mounted set |
| `ZIPMOUNT E C:\GAMES\GAME.ZIP` | Mount a ZIP with persistent overlay on free E: |
| `ZIPMOUNT E C:\GAMES\GAME.ZIP -ro` | Mount read-only, including existing overlay changes |
| `ZIPMOUNT E -u` | Unmount that ZIP drive |
| `FORMAT A: /Q` | Destructive quick format of recognizable floppy media |
| `FORMAT A: /U` | Destructive full format at existing floppy capacity |
| `VIDEOMOD /P` | Page the mode listing |
| `VIDEOMOD /HOST` | List advertised host modes |
| `VIDEOMOD /DOS` | List DOS BIOS/VESA modes |

### Tools and networking commands

| Command | Purpose |
| --- | --- |
| `DZ` | Start Doszip Commander |
| `BENCH` | Open benchmark menu; Q returns |
| `DEMOS` | Open demo menu; H gives notes, Q returns |
| `NETINFO` | Inspect DOS network settings/status |
| `NETUP` | Retry saved DOS network startup |
| `IPXNET` | Manual IPX tunnel facilities; see installed help |
| `WFWNET` | Prepare the configured WfW network stack before WIN |

For mTCP arguments and network-driver combinations, read NETWORK.TXT and the documentation under `Z:\NETWORK\MTCP`.

### Commands you should not assume are present

Pi-DOS deliberately excludes LS, TITLE, CAPMOUSE, INTRO, SHUTDOWN.COM, BUFFERS.COM, FDISK, CFGTOOL, DSXMENU, EDLIN, COMP, FILES, FCBS, LASTDRIV and FLAGSAVE. EDIT and FC remain.

Use REBOOT and POWEROFF for lifecycle control. Do not use an unrelated MS-DOS manual as proof that a particular maintenance, compression, undelete or development utility is bundled.

## Appendix B Drives, Directories and Defaults

Use this appendix to locate important files and compare an installation with fresh-image settings. Saved choices on an existing card may differ.

### Important paths

| Path | Contents or role |
| --- | --- |
| `C:\AUTOEXEC.BAT` | Personal startup commands |
| `C:\README.TXT` | Configuration/startup guidance |
| `C:\SYSTEM\PIDOS.CFG` | Authoritative saved configuration |
| `C:\SYSTEM\PIDOS.BAK` | Immediately previous setup configuration |
| `C:\SYSTEM\PIDOS.TMP` | Save/recovery temporary file |
| `C:\SYSTEM\CONTROL.REQ` | Supervised batch lifecycle request; managed |
| `C:\SYSTEM\CAPTURES` | Numbered screenshots |
| `C:\SYSTEM\MIDI` | Supplied internal MIDI assets |
| `C:\SYSTEM\NET` | Generated DOS networking files |
| `C:\SYSTEM\BENCH` | Benchmark menu and payload |
| `C:\PROGRAMS\TOOLS` | Writable editor and personal tools |
| `C:\PROGRAMS\DOSZIP` | File manager, preferences and notices |
| `C:\GAMES`, `C:\DEMOS` | User games and demo pack |
| `C:\DOCS`, `C:\TEMP` | Documentation and temporary files |
| `Z:\NETWORK` | Installed networking tools and manual |

The directory C:\IMAGES used in examples is a suggested user-created location, not a guaranteed pre-created directory.

### Fresh-image defaults

| Area | Current default |
| --- | --- |
| CPU | Pi 3: 486DX2/66; Pi 4: Pentium 75; Pi 5: Pentium 120. See [Selecting a CPU profile](#selecting-a-cpu-profile). |
| DOS RAM | 16 MiB |
| Video | S3 Trio64 |
| Voodoo | Software enabled, maximum memory, Glide passthrough off |
| Monitor | CRT |
| Retained LCD preferences | 1024x768, 4:3, linear filtering off |
| PC sound | SB Pro 2, 220h, IRQ 7, DMA 1 |
| PC speaker | Enabled |
| MIDI | SC-55 SoundFont profile, assets required |
| MPU-401 | Intelligent, port 330h, automatic IRQ |
| Physical audio | Primary HDMI |
| Keyboard / country | Spanish `sp` / Spain `34` |
| Timezone | Europe/Madrid |
| Joysticks | Two conventional two-axis devices, timed axes, autofire off |
| DOS IPX / NE2000 | Disabled / disabled |
| Diagnostics | Disabled |

Earlier development defaults included Pentium 133, Automatic CPU and SB16. Those are historical settings, not this edition's fresh-image defaults.

### Startup PATH

The standard search path is:

```dos
PATH C:\PROGRAMS\TOOLS;C:\SYSTEM;Z:\;Z:\SYSTEM;Z:\DOS;Z:\BIN;Z:\TEXTUTIL;Z:\DEBUG;Z:\PATCHING;Z:\4DOS
```

Writable startup sets TEMP and TMP to C:\TEMP. Extend the existing path in AUTOEXEC.BAT when adding your tools rather than inadvertently discarding the supplied locations.

### Configuration ownership

PIDOSCFG edits supported values in PIDOS.CFG. AUTOEXEC.BAT contains personal commands. SYSTEM\NET holds generated network files. The host creates transient runtime configuration separately; editing generated files is not the normal way to save settings.

C: and Z: mappings and their DOS labels are protected. The host FAT32 data partition must remain labelled PI-DOS even though DOS shows C: as SYSTEM.

## Appendix C Glossary

### A to I

| Term | Meaning |
| --- | --- |
| 8.3 name | A classic DOS name with up to eight base characters and three extension characters |
| ALSA | The native audio and MIDI interface used by the Raspberry Pi side |
| Attribute | A DOS file property such as read-only, hidden, system or archive |
| Batch file | A BAT text file containing shell commands |
| Boot image | A complete system image written to a storage device |
| CPU profile | A global choice of emulated CPU model, core and speed policy |
| Directory | A container for files and subdirectories |
| DOS country | A selection governing DOS regional conventions |
| DPMI | Protected-mode services used by some DOS applications and extenders |
| Dynrec | Dynamic translation of guest x86 instructions into native Arm code |
| EDID | Information describing display/adapter modes and capabilities |
| EMS / XMS | Expanded / extended memory interfaces for DOS software |
| Guest | The emulated x86 PC and its DOS environment |
| Host | The native Raspberry Pi system and physical device layer |
| Image mount | Making a disk-image file available as an emulated drive |
| Initramfs | The Linux root environment embedded inside Image |

<!-- PAGEBREAK -->

### K to Z

| Term | Meaning |
| --- | --- |
| KMS/DRM | The native display mode and presentation interface |
| MIDI | Musical performance messages sent to a synthesizer or module |
| MSCDEX | The DOS CD-ROM interface used by programs |
| Overlay | Persistent changes stored beside a ZIP in its PFS directory |
| Packet driver | A DOS networking interface used by tools such as mTCP |
| PATH | Directories searched for executable commands |
| PCAP | Direct guest Ethernet access through a wired host interface |
| Root directory | The top directory on a drive, written with a backslash |
| SLIRP | NAT-style networking for guest TCP/IP |
| SoundFont | Sample-based synthesis data used by the internal MIDI profiles |
| Supervisor | Pi-DOS's native startup and lifecycle controller |
| TSR | A DOS program that terminates but remains resident |
| Volume label | A name stored for a filesystem or presented by a drive |

## Appendix D Technical and Distribution Notes

This appendix gives context for advanced users. Normal DOS operation does not require building the image or administering the underlying Linux layer.

### How the system fits together

Raspberry Pi firmware starts an ARM64 Linux kernel with an embedded initramfs. A custom native supervisor mounts data, discovers devices, reads settings and starts the DOS runtime. The runtime derives from DOSBox-X and supplies the emulated x86 processor, BIOS, DOS shell and PC devices.

Native SDL2 presents fullscreen video through KMS/DRM and GLES, without X11, Wayland or a desktop. ALSA supplies real audio and MIDI transport. USB input and storage adapters connect the physical equipment to DOS-visible devices.

Linux still schedules work and drives the hardware. Pi-DOS's dedicated DOS-machine experience does not mean x86 instructions execute natively on Arm or that the system is electrically compatible with an ISA PC.

### Storage layout

| Component | Initial layout |
| --- | --- |
| BOOT | 256 MiB FAT32, starting at 1 MiB |
| PI-DOS | 512 MiB FAT32, expanded where the expected layout permits |
| Native system root | Embedded in Image, unpacked at boot |
| Temporary runtime state | RAM-backed paths |

There is no separate writable Linux root partition. Pi-DOS attempts to remount the volatile system root read-only. BOOT and PI-DOS remain ordinary FAT partitions; the whole card is not immutable.

Expansion checks the expected signature, sector size, partition starts, label and absence of extra partitions. It does not format an unexpected device. A failed check can lead to read-only or recovery handling. Physical power interruption during filesystem metadata changes can still damage FAT32.

### Building on a development computer

The project uses a Buildroot external tree and pinned upstream sources. Image building requires the documented development-host tools and dependencies. These commands are entered on that development computer, not at the Pi-DOS prompt:

```sh
make image-config
JOBS=4 make image
```

The diagnostic alternative is `JOBS=4 make image-diagnostic`. Images are produced under `output/buildroot-arm64/images/`. The build does not flash a physical card or update EEPROM automatically.

User-supplied MIDI assets are staged with the project's MIDI asset importer and must match its manifest. A runtime that works offline does not imply an offline build without cached source archives and assets.

Use the project's technical reference and build instructions for dependency versions, patch maintenance, rebuilding setup and verification. The complete development history belongs there rather than in this user guide.

### Software and asset notices

The reviewed baseline had not selected a project-wide license for original Pi-DOS code. This guide does not select one. Components and bundled payloads retain their individual licenses, notices and distribution conditions.

DOSBox-X modifications, Doszip, mTCP, SDL2, firmware and other components have their own source/notice requirements. A public image release needs a complete component and asset review. Source availability for an engine does not automatically cover commercial game data, shareware payloads, MIDI ROMs or SoundFonts.

Preserve the original notices included with tools and data. Consult the repository's LICENSES documentation before redistributing an image or payload collection. Normal use of this guide does not grant new rights to third-party software.

### Editorial basis

This guide was prepared from the supplied `technical-reference.md`, reviewed 30 October 2026, covering Pi-DOS 1.0 at baseline `ff0c194`. It does not claim that new hardware tests were performed during writing.

The supplied 1993 Microsoft MS-DOS 6 Concise User's Guide was used as a reference for organization, typography and procedure layout. This guide is independently written for Pi-DOS and does not imply Microsoft endorsement or that Microsoft DOS is included.

## Appendix E Compatibility and Verification Notes

Implemented support, host checks and physical confirmation are different kinds of evidence. This appendix keeps the distinction visible without interrupting every everyday procedure.

### What the reference records

| Area | Recorded evidence and practical boundary |
| --- | --- |
| Board boot and input | User-confirmed Pi 3 and Pi 5, and Pi 4 after matching firmware correction; other targets need checks |
| Reboot and persistence | Appliance REBOOT, persisted test file and POWEROFF confirmed; power-loss recovery not established |
| PC execution | Native/Pi 5 COM, EXE, BAT and dynamic-recompiler checks; not a complete application suite |
| Display | Pi 5 GPU/KMS checks and user CRT appearance report; each timing/display and latest static welcome need checks |
| Keyboard/mouse | Virtual keyboard-path checks and user hotplug record; every application not certified |
| Controllers | Virtual controller checks; physical game calibration and hotplug pending |
| Audio | Short speaker/FM/SB checks; long audible, analogue-jack and USB acceptance pending |
| MIDI | Profile, identity and retry implementation; Automatic-CPU Sierra timing issue remains open |
| Floppy | Specific Pi 5 Toshiba/TEAC copy and format confirmation; other drives and later metadata changes need checks |
| Optical / USB data | Host/backend checks and ARM64 integration; physical media, audio and removal acceptance pending |
| ZIP / sharing / attributes | Focused DOS/backend checks; every application and physical device not certified |
| Networking | Configuration, host/backend and packaging checks; physical Pi LAN, native IPX and WfW sharing pending |
| Expansion | Native sparse-image checks at 2 and 64 GiB; ARM64 refresh and physical-card acceptance pending |
| Benchmarks / demos | Archive, inventory and launch-file checks; physical scores and playback synchronization pending |
| Screenshots / image cycling | Dispatch/configuration/image checks; physical Pi capture and in-game cycling pending |

### Reporting a new result

Record the exact Pi-DOS build, Raspberry Pi model, firmware/boot assets, peripheral model, settings, media and steps. State what you observed rather than extending one success to a whole family of devices.

A successful compile or image inventory check establishes packaging, not a visible display, audible sound or correct game timing. A successful file copy on one floppy drive does not establish all formats or all later metadata paths.

Use a disposable test file or backup copy when checking writes. For normal work, prefer a configuration already known to suit your software and equipment. Keep new results with the project's technical documentation so a later guide can describe them accurately.



## Index

Use this alphabetical index to find a subject. Section links are clickable in the Markdown and PDF editions; the PDF also gives page references.

| Subject | Section |
| --- | --- |
| 8.3 filenames | [Filenames and extensions](#filenames-and-extensions) |
| Application compatibility | [Checking compatibility methodically](#checking-compatibility-methodically) |
| Attributes | [File attributes and open files](#file-attributes-and-open-files) |
| Audio output | [Choosing the physical output](#choosing-the-physical-output) |
| AUTOEXEC.BAT | [Customizing user startup](#customizing-user-startup) |
| Backups, configuration | [Setup backups and interrupted saves](#setup-backups-and-interrupted-saves) |
| Backups, user files | [Backing up your work](#backing-up-your-work) |
| Benchmarks | [Running benchmarks](#running-benchmarks) |
| Board support | [What you need](#what-you-need) |
| Boot image preparation | [Preparing the system card](#preparing-the-system-card) |
| C: unavailable | [When the system reaches Z: instead of C:](#when-the-system-reaches-z-instead-of-c) |
| CD images | [Mounting a CD image](#mounting-a-cd-image) |
| CDs, physical | [Using physical CDs](#using-physical-cds) |
| Clock synchronization | [Reviewing date and time](#reviewing-date-and-time) |
| Command inventory | [Appendix A Command Quick Reference](#appendix-a-command-quick-reference) |
| CONFIG.SYS | [CONFIG.SYS and familiar MS-DOS advice](#configsys-and-familiar-ms-dos-advice) |
| Controllers | [Choosing a joystick or gamepad mode](#choosing-a-joystick-or-gamepad-mode) |
| Copying files | [Copying and renaming a file](#copying-and-renaming-a-file) |
| CPU profiles | [Selecting a CPU profile](#selecting-a-cpu-profile) |
| CRT displays | [Using a CRT monitor](#using-a-crt-monitor) |
| Date and time | [Reviewing date and time](#reviewing-date-and-time) |
| Deleting files | [Deleting the practice files](#deleting-the-practice-files) |
| Demos | [Running demos](#running-demos) |
| Diagnostic logs | [Collecting diagnostic logs](#collecting-diagnostic-logs) |
| Directories, creating | [Creating and changing directories](#creating-and-changing-directories) |
| Directories, system map | [Important paths](#important-paths) |
| Disc switching | [Using more than one image](#using-more-than-one-image) |
| DOS basics | [Chapter 2 Learning DOS Basics](#chapter-2-learning-dos-basics) |
| Doszip Commander | [Using Doszip Commander](#using-doszip-commander) |
| Drive letters | [Understanding drive assignments](#understanding-drive-assignments) |
| EDIT | [Creating a small text file](#creating-a-small-text-file) |
| External MIDI | [Connecting an external MIDI module](#connecting-an-external-midi-module) |
| Factory defaults | [Fresh-image defaults](#fresh-image-defaults) |
| File sharing and locks | [When a program reports a sharing or lock error](#when-a-program-reports-a-sharing-or-lock-error) |
| First-boot setup | [Completing the welcome screens](#completing-the-welcome-screens) |
| Floppy disks | [Using a USB floppy drive](#using-a-usb-floppy-drive) |
| Formatting floppies | [Formatting a floppy](#formatting-a-floppy) |
| Game timing | [Fixed speed and Automatic](#fixed-speed-and-automatic) |
| Image sets | [Using more than one image](#using-more-than-one-image) |
| IPX multiplayer | [Playing through an IPX tunnel](#playing-through-an-ipx-tunnel) |
| Keyboard layout | [Changing regional settings](#changing-regional-settings) |
| LCD displays | [Using a flat-panel display](#using-a-flat-panel-display) |
| Licenses and assets | [Software and asset notices](#software-and-asset-notices) |
| Long filenames | [Filenames and extensions](#filenames-and-extensions) |
| Memory | [Choosing DOS memory](#choosing-dos-memory) |
| MIDI assets | [Choosing a MIDI profile](#choosing-a-midi-profile) |
| Migration | [Moving from an older layout](#moving-from-an-older-layout) |
| Mixer | [Adjusting the mixer](#adjusting-the-mixer) |
| Network, direct LAN | [Connecting directly to a wired LAN](#connecting-directly-to-a-wired-lan) |
| Network, TCP/IP | [Enabling DOS TCP/IP with SLIRP](#enabling-dos-tcpip-with-slirp) |
| PATH | [Customizing user startup](#customizing-user-startup) |
| Paths | [Understanding paths](#understanding-paths) |
| PIDOS.CFG | [Understanding PIDOS.CFG](#understanding-pidoscfg) |
| PIDOSCFG controls | [Opening setup](#opening-setup) |
| Poweroff | [Turning off or restarting](#turning-off-or-restarting) |
| Read-only storage | [When C: is read-only](#when-c-is-read-only) |
| Reboot | [Turning off or restarting](#turning-off-or-restarting) |
| Recovery | [Chapter 12 Diagnosing and Solving Problems](#chapter-12-diagnosing-and-solving-problems) |
| Regional settings | [Changing regional settings](#changing-regional-settings) |
| Screenshots | [Taking a screenshot](#taking-a-screenshot) |
| Shortcuts | [System shortcut keys](#system-shortcut-keys) |
| Sound card setup | [Choosing a PC sound device](#choosing-a-pc-sound-device) |
| Timezone | [Changing regional settings](#changing-regional-settings) |
| Updates | [Updating a compatible installation](#updating-a-compatible-installation) |
| USB data storage | [Using USB sticks and external disks](#using-usb-sticks-and-external-disks) |
| Validation status | [Appendix E Compatibility and Verification Notes](#appendix-e-compatibility-and-verification-notes) |
| Version information | [Your first command prompt](#your-first-command-prompt) |
| Video modes | [Inspecting available modes](#inspecting-available-modes) |
| Volume labels | [Labels and media safety](#labels-and-media-safety) |
| Wildcards | [Using wildcards](#using-wildcards) |
| Windows for Workgroups | [Windows for Workgroups](#windows-for-workgroups) |
| ZIP drives | [Mounting a ZIP as a writable drive](#mounting-a-zip-as-a-writable-drive) |
| ZIP saves and PFS overlays | [Keeping ZIP saves](#keeping-zip-saves) |
