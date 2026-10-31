// Generated from pi-dos-user-guide.md by build-guide.py.
// Edit the Markdown for content; edit TEMPLATE in the builder for shared styling.
#set document(title: "Pi-DOS 1.0 User's Guide", author: "Pi-DOS", date: datetime(year: 2026, month: 10, day: 30))
#set text(font: "Nimbus Roman", size: 10.5pt, lang: "en", hyphenate: true)
#set par(justify: true, leading: 4.6pt, first-line-indent: 0pt, spacing: 5.5pt)
#set page(width: 155mm, height: 230mm, margin: (inside: 20mm, outside: 17mm, top: 21mm, bottom: 18mm), binding: left, header: none, footer: none)
#set heading(numbering: none)
#set enum(indent: 0pt, body-indent: 5mm, spacing: 4pt)
#set list(indent: 0pt, body-indent: 4mm, spacing: 4pt)
#set table(inset: (x: 3.2pt, y: 4.2pt), stroke: none)
#let running = state("running", "Contents")
#let p(body, sticky: false) = block(inset: (left: 10mm), above: 0pt, below: 5.5pt, sticky: sticky, body)
#let inline-code(s) = text(font: "Nimbus Mono PS", size: 0.87em, hyphenate: false, s)
#let code-block(s) = block(inset: (left: 10mm), above: 2pt, below: 8pt)[
  #set par(justify: false, leading: 2.5pt)
  #text(font: "Nimbus Mono PS", size: 9.3pt, weight: "bold", hyphenate: false, s)
]
#let procedure(body) = block(sticky: true, above: 7pt, below: 4pt)[
  #grid(columns: (10mm, 1fr), align: (left, left), [#h(3mm)#polygon(fill: black, stroke: none, (0pt, 0pt), (6pt, 3.5pt), (0pt, 7pt))], [#text(weight: "bold", body)])
]
#let notice(body) = block(inset: (left: 10mm), above: 4pt, below: 8pt)[
  #set par(justify: true)
  #body
]
#let items(body) = block(inset: (left: 10mm), above: 0pt, below: 6pt, breakable: false, body)
#let reference-table(cols, cells, compact: false) = block(above: 3pt, below: 9pt, breakable: cells.len() > 14)[
  #set text(size: if compact { 9pt } else { 9.4pt })
  #set table(inset: (x: 3.2pt, y: if compact { 3pt } else { 4.2pt }))
  #set par(justify: false, leading: 3.5pt)
  #table(columns: cols, table.header(..cells.slice(0, cols.len()).map(c => table.cell(fill: luma(94%), text(weight: "bold", c)))), table.hline(stroke: 0.45pt), ..cells.slice(cols.len()), table.hline(stroke: 0.45pt))
]
#show heading.where(level: 1): it => {
  running.update(it.body)
  block(above: 5mm, below: 7mm, sticky: true)[
    #set text(size: 24pt, weight: "bold", hyphenate: false)
    #set par(justify: false, leading: 3pt)
    #it.body
  ]
}
#show heading.where(level: 2): it => block(above: 9pt, below: 5pt, sticky: true)[
  #set text(size: 16pt, weight: "bold", hyphenate: false)
  #set par(justify: false, leading: 3.5pt)
  #it.body
]
#show link: set text(fill: black)
#let book-header() = context {
  let n = counter(page).get().first()
  let page-label = counter(page).display()
  let current-chapters = query(heading.where(level: 1)).filter(h => h.location().page() == here().page())
  let title = if current-chapters.len() > 0 { current-chapters.first().body } else { running.get() }
  set text(size: 8.6pt, weight: "bold", hyphenate: false)
  set par(justify: false)
  if calc.even(n) {
    grid(columns: (8mm, 1fr), page-label, [Pi-DOS 1.0 User's Guide])
  } else {
    grid(columns: (1fr, 8mm), align(right, title), align(right, page-label))
  }
  v(2pt)
  line(length: 100%, stroke: 0.45pt)
}

// Cover: deliberately monochrome, with the generous spacing of a printed manual.
#v(9mm)
#text(size: 16pt, weight: "bold", hyphenate: false)[User's Guide]
#v(25mm)
#text(size: 46pt, weight: "bold")[Pi-DOS]
#v(3mm)
#text(size: 20pt)[Version 1.0]
#v(7mm)
#line(length: 100%, stroke: 1.1pt)
#v(5mm)
#text(size: 14pt)[A DOS-compatible computer\ for Raspberry Pi]
#v(1fr)
#text(size: 10pt)[Edition 1\ 30 October 2026]
#pagebreak()

// Verso / edition note.
#v(12mm)
#text(size: 17pt, weight: "bold")[Pi-DOS 1.0 User's Guide]
#v(6mm)
#text(size: 10pt)[Edition 1 · 30 October 2026\ Technical baseline: ff0c194]
#v(8mm)
#text(size: 10pt)[Prepared from the supplied Pi-DOS technical reference. This edition describes the reviewed implementation; it does not claim new physical hardware validation.]
#v(4mm)
#text(size: 10pt)[Names of example programs and files are illustrative. Games, commercial software and some MIDI assets must be supplied separately where required.]
#v(4mm)
#text(size: 10pt)[The organization and page design are inspired by the 1993 Microsoft MS-DOS 6 Concise User's Guide. The prose and procedures are written for Pi-DOS. Microsoft endorsement is not implied.]
#v(4mm)
#text(size: 10pt)[Component and asset licenses remain independent. No project-wide license is selected by this guide. See Appendix D and the project's notices.]
#v(1fr)
#line(length: 100%, stroke: 0.45pt)
#v(3mm)
#text(size: 9pt)[155 × 230 mm · Monochrome edition]
#pagebreak()

// Contents and front matter use Roman numbering; the chapters restart at 1.
#counter(page).update(3)
#set page(numbering: "i", header: book-header(), footer: none)
#running.update([Contents])
#text(size: 24pt, weight: "bold", hyphenate: false)[Contents]
#v(6mm)
#{
  set text(size: 9.5pt)
  set par(justify: false, leading: 3.5pt)
  show outline.entry: it => {
    if it.level == 1 { v(4pt); strong(it) } else { it }
  }
  outline(title: none, depth: 2, indent: 5mm)
}

#pagebreak(weak: true)

#heading(level: 1)[#text("Welcome")]#label("h-welcome")

#p[#text("Welcome to Pi-DOS. With a Raspberry Pi, a keyboard and a display, you can use a computer that starts at a familiar DOS prompt. You can organize files, run compatible DOS programs, connect removable media and choose the kind of PC hardware your software expects.")]

#p[#text("Pi-DOS combines a dedicated Raspberry Pi system with an emulated x86 computer. DOS programs see PC processors, memory, video adapters and sound devices. The Raspberry Pi provides the real display connections, USB ports, storage and audio outputs. You normally work entirely from DOS; there is no desktop or Linux login to pass through.")]

#p[#text("You do not need to understand the underlying system to begin. This guide starts with connecting your equipment and reaching the prompt, then introduces files, programs and setup. Later chapters cover sound, removable disks, networking and troubleshooting.")]

#heading(level: 2)[#text("About this guide")]#label("h-about-this-guide")

#p[#text("This edition describes Pi-DOS 1.0 at implementation baseline ")#inline-code("ff0c194")#text(". Existing installations may retain earlier settings. When this guide says ")#emph[#text("default")]#text(", it means the fresh-image setting at that baseline, unless stated otherwise.")]

#p[#text("This is a user guide rather than a record of development. It explains implemented behavior, but does not certify every Raspberry Pi, peripheral, game or Windows installation. Appendix E summarizes the hardware checks recorded in the technical reference and identifies features that still need physical confirmation.")]

#p[#text("The supplied technical reference is the authority for Pi-DOS-specific behavior. The organization and restrained print design take inspiration from the 1993 Microsoft MS-DOS 6 Concise User's Guide. The explanations and procedures here are written for Pi-DOS; an MS-DOS command or utility is not assumed to exist just because an older manual describes it.")]

#heading(level: 2)[#text("Finding the information you need")]#label("h-finding-the-information-you-need")

#p[#text("If you are new to DOS, read Chapters 1 through 3 in order. If you already know DOS, read Chapter 1 and the Pi-DOS differences in Chapter 4, then use the hardware and media chapters as needed.")]

#reference-table((0.85fr, 1.15fr), (
[#text("To learn about")],
[#text("Read")],
[#text("Connecting equipment, preparing a card and first startup")],
[#text("Chapter 1")],
[#text("Commands, paths and a short DOS tutorial")],
[#text("Chapter 2")],
[#text("Copying files, running programs and making backups")],
[#text("Chapter 3")],
[#text("Setup, saved configuration and user startup")],
[#text("Chapter 4")],
[#text("Processor speed and memory")],
[#text("Chapter 5")],
[#text("CRT and LCD displays")],
[#text("Chapter 6")],
[#text("Sound cards, physical audio and MIDI")],
[#text("Chapter 7")],
[#text("USB storage, floppies, CDs, images and ZIP drives")],
[#text("Chapter 8")],
[#text("Keyboard, mouse, gamepads and shortcuts")],
[#text("Chapter 9")],
[#text("Clock, regional settings and networking")],
[#text("Chapter 10")],
[#text("Benchmarks, demos and screenshots")],
[#text("Chapter 11")],
[#text("Recovery, troubleshooting and updates")],
[#text("Chapters 12 and 13")],
[#text("Commands, defaults and technical background")],
[#text("Appendices A through E")],
), compact: false)

#heading(level: 2)[#text("Conventions used in this guide")]#label("h-conventions-used-in-this-guide")

#p[#text("A ")#emph[#text("drive")]#text(" is a storage location identified by a letter, such as ")#inline-code("C:")#text(". A ")#emph[#text("directory")]#text(", also called a folder, holds files and other directories. A ")#emph[#text("path")]#text(" identifies a file or directory by its location.")]

#p[#text("Text in ")#inline-code("monospaced type")#text(" represents commands, filenames, paths, configuration entries or messages. Bold text emphasizes controls and important choices. A triangle marks the start of a procedure. A note gives additional information; an important notice calls attention to a consequence such as data loss.")]

#p[#text("When a procedure says to type a command, type the command and press ")#strong[#text("Enter")]#text(". Do not type a displayed prompt such as ")#inline-code("C:\\>")#text(". Commands on separate lines are entered separately. Example files and directories are illustrative; substitute your own names where appropriate.")]

#p[#text("")#strong[#text("Ctrl+Alt+Delete")]#text(" means hold Ctrl and Alt, press Delete, then release the keys. On Pi-DOS this normally requests a clean reboot of the whole Raspberry Pi. It is not a way to return safely from a game with unsaved progress.")]

#heading(level: 2)[#text("Before you begin")]#label("h-before-you-begin")

#p[#text("Have a copy of the Pi-DOS image intended for your board, suitable storage and a way to write the image from another computer. Keep that other computer available for copying files or repairing a card that Pi-DOS cannot mount.")]

#p[#text("Pi-DOS supplies a DOS-compatible environment, not an original Microsoft DOS installation. Games, commercial applications, Windows, MIDI ROMs and some sound assets may need to be supplied separately. Use software and assets you are entitled to use, and retain their accompanying instructions.")]

#pagebreak(to: "odd")
#counter(page).update(1)
#set page(numbering: "1", header: book-header())

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 1 Getting Started")]#label("h-chapter-1-getting-started")

#p[#text("This chapter helps you prepare the equipment, start Pi-DOS and complete its welcome screens. Before changing an existing card, make a backup of files you want to keep.")]

#heading(level: 2)[#text("What you need")]#label("h-what-you-need")

#p[#text("Use a supported 64-bit Raspberry Pi, an appropriate power supply, an SD card or supported boot arrangement, an HDMI display or suitable adapter, and a wired USB keyboard. A USB mouse is useful for some applications and for setup. A wired Ethernet connection is optional; normal DOS use does not require Internet access.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Raspberry Pi family")],
[#text("Image targets")],
[#text("Pi 2")],
[#text("Model B revision 1.2 only")],
[#text("Pi 3")],
[#text("3B, 3B+ and 3A+")],
[#text("Pi Zero")],
[#text("Zero 2 W only")],
[#text("Pi 4")],
[#text("4B and 400")],
[#text("Pi 5")],
[#text("5, 500 and 500+")],
[#text("Compute Modules")],
[#text("Corresponding 64-bit variants with suitable carrier and USB host hardware")],
), compact: false)

#p[#text("The original Pi Zero, Zero W and Pi 2 revision 1.1 are excluded. A target in this list is not a promise that every carrier, peripheral and display combination has been tested. Basic boot and input are recorded for Pi 3 and Pi 5, and Pi 4 with corrected matching firmware.")]

#p[#text("Wi-Fi and Bluetooth are not configured by this image. Use wired USB input and, when needed, wired Ethernet. Zero and Compute Module systems need appropriate USB host adapters or carrier hardware.")]

#heading(level: 2)[#text("Preparing the system card")]#label("h-preparing-the-system-card")

#p[#text("The development image starts at about 769 MiB. Its data partition is designed to expand into available card space during startup. Choose a card comfortably larger than the image and large enough for your programs, disk images and backups.")]

#procedure[#text("To write a new Pi-DOS card")]

#items[#enum(start: 1, [#text("On another computer, obtain the intended ")#inline-code("pi-dos-arm64-sdcard.img")#text(" image, or its diagnostic variant.")], [#text("Insert the destination card and open your image-writing utility.")], [#text("Select the Pi-DOS image and carefully select the destination device.")], [#text("Write and, if your utility offers it, verify the image.")], [#text("Eject the card using that computer's normal removal procedure.")])]

#notice[#text("")#strong[#text("Important")]#text(" Writing an image replaces the destination card's existing partition layout and data. Confirm the device before writing. This is a preparation task on another computer, not a DOS FORMAT operation.")]

#p[#text("A freshly written card contains a ")#inline-code("BOOT")#text(" partition and a FAT32 partition labelled ")#inline-code("PI-DOS")#text(". Do not rename the PI-DOS partition: Pi-DOS uses that label to find its persistent files. The DOS label displayed for C: is SYSTEM; this is intentional and is different from the host partition label.")]

#heading(level: 2)[#text("Connecting your equipment")]#label("h-connecting-your-equipment")

#procedure[#text("To connect and start Pi-DOS")]

#items[#enum(start: 1, [#text("With the Raspberry Pi off, insert the prepared system card.")], [#text("Connect the display and USB keyboard. Connect a mouse if you intend to use one.")], [#text("Connect any USB audio or external MIDI device you intend to select at startup.")], [#text("Connect wired Ethernet if you want network clock synchronization or networking.")], [#text("Turn on the display and apply power to the Raspberry Pi.")])]

#p[#text("On a dual-HDMI board, default HDMI audio uses HDMI0, the connector nearest the USB-C power connector. Use that connection when relying on the default audio route.")]

#p[#text("Pi-DOS may check and expand its data partition before reaching DOS. Allow it to finish; do not remove power during a storage operation. Expansion is restricted to the expected Pi-DOS layout. Unexpected layouts are left unchanged rather than formatted.")]

#p[#text("The startup table reports the physical board and selected emulated PC hardware. Its CPU, DOS memory, video and sound entries describe the configured environment. They do not mean that those components exist as physical PC expansion cards.")]

#heading(level: 2)[#text("Completing the welcome screens")]#label("h-completing-the-welcome-screens")

#p[#text("Fresh images normally run a two-stage setup. The initial defaults are a Spanish keyboard, Spain as the DOS country, the Europe/Madrid timezone and a CRT monitor profile. Set these to match your equipment and location.")]

#procedure[#text("To complete first-boot setup")]

#items[#enum(start: 1, [#text("In the welcome screen, select your keyboard layout, DOS country and timezone.")], [#text("Choose CRT or LCD. For an LCD, select a fixed output resolution supported by your display.")], [#text("Press ")#strong[#text("F2")]#text(" to save. The startup welcome requests a clean reboot to apply these choices.")], [#text("After reboot, use the typing test to check your keyboard and review the local date and time.")], [#text("Press ")#strong[#text("F2")]#text(" or ")#strong[#text("Esc")]#text(" to finish the review and continue.")])]

#p(sticky: true)[#text("On the initial welcome screen, ")#strong[#text("Esc")]#text(" skips setup, discards pending edits and continues with saved defaults. You can reopen the welcome later:")]

#code-block("PIDOSCFG /FIRSTBOOT")

#p[#text("When you launch it manually, saving regional settings still requires you to run ")#inline-code("REBOOT")#text(" explicitly. A save that fails does not complete the pending setup stage.")]

#p[#text("Time may be incorrect before network synchronization finishes. Pi-DOS does not hold up DOS startup while waiting for a connection. Offline time depends on an available working real-time clock. Chapter 10 explains manual clock changes and synchronization.")]

#heading(level: 2)[#text("Your first command prompt")]#label("h-your-first-command-prompt")

#p(sticky: true)[#text("Normal startup ends at:")]

#code-block("C:\\​>")

#p(sticky: true)[#text("Type the following to see the files and directories on C:")]

#code-block("DIR")

#p(sticky: true)[#text("To identify the running product, type:")]

#code-block("VER")

#p[#text("If you see a Z: recovery prompt instead, the persistent data partition could not be used. Do not assume your files have been erased. See Chapter 12 before changing the card.")]

#heading(level: 2)[#text("Turning off or restarting")]#label("h-turning-off-or-restarting")

#procedure[#text("To turn off Pi-DOS")]

#items[#enum(start: 1, [#text("Save your work in the running program.")], [#text("Exit the program to the DOS prompt.")], [#text("Type ")#inline-code("POWEROFF")#text(" and allow the shutdown sequence to finish before disconnecting power.")])]

#p(sticky: true)[#text("For a clean restart, type:")]

#code-block("REBOOT")

#p[#text("The default ")#strong[#text("Ctrl+Alt+Delete")]#text(" shortcut also requests a clean whole-machine reboot, including while a program is running. Save first: flushing storage cannot save an application's unsaved document or game state.")]

#p[#text("A Pi 5 short power-button press is implemented as another clean shutdown request, though the technical reference records physical acceptance of that button path as pending. Use the command as the primary documented procedure. A long hold forces power off and can interrupt writes.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 2 Learning DOS Basics")]#label("h-chapter-2-learning-dos-basics")

#p[#text("At the command prompt, you tell Pi-DOS what to do by typing commands. Most daily tasks involve selecting a drive, changing directories and naming the files you want to use.")]

#p[#text("This tutorial uses a new directory called GUIDE on C:. If you already have a directory with that name, substitute a different unused name.")]

#heading(level: 2)[#text("Understanding the prompt")]#label("h-understanding-the-prompt")

#p[#text("In ")#inline-code("C:\\>")#text(", C: is the selected drive and the backslash is its root directory. When you change to a subdirectory, the prompt normally shows that location. DOS commands and paths are generally case-insensitive.")]

#p[#text("You can use uppercase or lowercase when entering the commands in this chapter. Pi-DOS normally displays uppercase short names even when the underlying file has a longer name.")]

#heading(level: 2)[#text("Viewing a directory")]#label("h-viewing-a-directory")

#procedure[#text("To see the contents of the current directory")]

#items[#enum(start: 1, [#text("At the prompt, type ")#inline-code("DIR")#text(".")], [#text("Read the displayed file and directory names.")], [#text("For a long listing, use the paged form below.")])]

#code-block("DIR /P")

#p(sticky: true)[#text("For a compact, wider listing, use:")]

#code-block("DIR /W")

#p[#text("A directory listing shows what is available at the selected location. An empty or disconnected removable drive can report ")#emph[#text("drive not ready")]#text(". That message does not mean the drive contains an empty disk.")]

#heading(level: 2)[#text("Creating and changing directories")]#label("h-creating-and-changing-directories")

#procedure[#text("To create a practice directory")]

#items[#enum(start: 1, [#text("Select C: by typing ")#inline-code("C:")#text(".")], [#text("Return to the root by typing ")#inline-code("CD \\")#text(".")], [#text("Create the directory and enter it:")])]

#code-block("MD GUIDE\nCD GUIDE")

#p[#inline-code("MD")#text(" makes a directory. ")#inline-code("CD")#text(" changes the current directory. A subdirectory is simply a directory inside another directory.")]

#procedure[#text("To create and enter a subdirectory")]

#items[#enum(start: 1, [#text("While in ")#inline-code("C:\\GUIDE")#text(", type ")#inline-code("MD NOTES")#text(".")], [#text("Type ")#inline-code("CD NOTES")#text(".")], [#text("Type ")#inline-code("CD ..")#text(" to return to GUIDE.")], [#text("Type ")#inline-code("CD \\")#text(" to return directly to the root of C:.")])]

#p[#text("The two periods in ")#inline-code("CD ..")#text(" mean the parent directory. A single backslash at the start of a path means the root of that drive.")]

#heading(level: 2)[#text("Creating a small text file")]#label("h-creating-a-small-text-file")

#p[#text("Pi-DOS includes an editor selected by the command ")#inline-code("EDIT")#text(". Its writable personalized copy normally lives in ")#inline-code("C:\\PROGRAMS\\TOOLS")#text(".")]

#procedure[#text("To create a practice note")]

#items[#enum(start: 1, [#text("Type the command below.")], [#text("Enter a short sentence in the editor.")], [#text("Use the editor's save and exit controls to save the file and return to DOS.")])]

#code-block("EDIT C:\\​GUIDE\\​HELLO.TXT")

#p(sticky: true)[#text("Now display the file:")]

#code-block("TYPE C:\\​GUIDE\\​HELLO.TXT")

#p[#text("Use the editor's own help for its editing keys. The selected editor is PEDIT Light, so do not assume that every key sequence from Microsoft's MS-DOS Editor applies.")]

#heading(level: 2)[#text("Copying and renaming a file")]#label("h-copying-and-renaming-a-file")

#procedure[#text("To copy your note into NOTES")]

#items[#enum(start: 1, [#text("Type ")#inline-code("CD \\GUIDE")#text(".")], [#text("Type the following command:")])]

#code-block("COPY HELLO.TXT NOTES\\​HELLO.TXT")

#p[#text("The first path names the source. The second names the destination. COPY leaves the source in place. If a destination file already exists, check the command's overwrite prompt before proceeding.")]

#procedure[#text("To rename the copied note")]

#items[#enum(start: 1, [#text("Type ")#inline-code("CD NOTES")#text(".")], [#text("Type ")#inline-code("REN HELLO.TXT FIRST.TXT")#text(".")], [#text("Type ")#inline-code("DIR")#text(" to see the new name.")])]

#p[#text("Renaming changes a name; it does not create a second copy. If the rename is refused, check whether a program still has the file open or whether a file with the new name already exists.")]

#heading(level: 2)[#text("Understanding paths")]#label("h-understanding-paths")

#reference-table((0.85fr, 1.15fr), (
[#text("Example")],
[#text("Meaning")],
[#inline-code("FIRST.TXT")],
[#text("A file in the current directory")],
[#inline-code("NOTES\\FIRST.TXT")],
[#text("A file in NOTES beneath the current directory")],
[#inline-code("\\GUIDE\\NOTES\\FIRST.TXT")],
[#text("A file reached from the root of the current drive")],
[#inline-code("C:\\GUIDE\\NOTES\\FIRST.TXT")],
[#text("A complete path on C:")],
[#inline-code("..")],
[#text("The parent of the current directory")],
), compact: false)

#p[#text("A full path is useful in instructions and batch files because it makes the intended location clear. When working with removable drives, verify the assigned letter before copying or deleting files.")]

#heading(level: 2)[#text("Selecting another drive")]#label("h-selecting-another-drive")

#p(sticky: true)[#text("To select an available drive, enter its letter followed by a colon:")]

#code-block("E:")

#p[#text("A: normally represents a USB floppy, D: a USB optical drive, and E: onward eligible USB data volumes or explicit image/ZIP mounts. C: and Z: are reserved. Selecting an automatic removable letter can succeed even if its media is not ready.")]

#p(sticky: true)[#text("Use the following command to inspect current mappings:")]

#code-block("MOUNT")

#heading(level: 2)[#text("Filenames and extensions")]#label("h-filenames-and-extensions")

#p[#text("Classic DOS names have up to eight characters before a period and up to three after it. The extension commonly indicates a file's role: TXT for text, BAT for batch commands, and COM or EXE for executable programs.")]

#p(sticky: true)[#text("Pi-DOS uses uppercase 8.3 aliases by default. It retains underlying long names and long-filename interfaces, but applications may choose their own presentation. To request the long-name directory view, type:")]

#code-block("DIR /-Z")

#p[#text("To return to the short-name view, type ")#inline-code("DIR /Z")#text(". Use the short name shown by DIR when an older application cannot handle the long name.")]

#heading(level: 2)[#text("Using wildcards")]#label("h-using-wildcards")

#p[#text("An asterisk matches a group of characters; a question mark matches one character in classic DOS matching. For example, ")#inline-code("DIR *.TXT")#text(" lists text files in the current directory. Use a listing first to check a wildcard before applying it to a command that changes files.")]

#notice[#text("")#strong[#text("Important")]#text(" A broad pattern such as ")#inline-code("*.*")#text(" can affect many files. This guide uses explicit filenames for destructive examples so the intended target is visible.")]

#heading(level: 2)[#text("Deleting the practice files")]#label("h-deleting-the-practice-files")

#procedure[#text("To remove only the tutorial files")]

#items[#enum(start: 1, [#text("Confirm that your original note is still in ")#inline-code("C:\\GUIDE")#text(".")], [#text("Delete the copied note, then remove its now-empty directory:")])]

#code-block("DEL C:\\​GUIDE\\​NOTES\\​FIRST.TXT\nRD C:\\​GUIDE\\​NOTES")

#items[#enum(start: 3, [#text("If you also want to remove the original note and practice directory, type:")])]

#code-block("DEL C:\\​GUIDE\\​HELLO.TXT\nCD \\\nRD C:\\​GUIDE")

#p[#inline-code("DEL")#text(" deletes a file. ")#inline-code("RD")#text(" removes an empty directory. Treat deletion as permanent unless you have a backup; this guide does not promise a bundled undelete tool.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 3 Managing Files and Programs")]#label("h-chapter-3-managing-files-and-programs")

#p[#text("C: is your normal persistent workspace. Use it for applications, documents and configuration. You can add files by connecting supported USB storage or by opening the PI-DOS partition on another computer after a clean shutdown.")]

#heading(level: 2)[#text("Knowing where files belong")]#label("h-knowing-where-files-belong")

#reference-table((0.85fr, 1.15fr), (
[#text("Directory")],
[#text("Normal use")],
[#inline-code("C:\\GAMES")],
[#text("Games and their data")],
[#inline-code("C:\\PROGRAMS")],
[#text("Applications and user tools")],
[#inline-code("C:\\DOCS")],
[#text("Documentation and personal text files")],
[#inline-code("C:\\DEMOS")],
[#text("Demoscene productions and menu content")],
[#inline-code("C:\\TEMP")],
[#text("Temporary files")],
[#inline-code("C:\\SYSTEM")],
[#text("Pi-DOS configuration, launchers and managed files")],
[#inline-code("C:\\SYSTEM\\CAPTURES")],
[#text("PNG screenshots")],
), compact: false)

#p[#text("Keep your own applications in named subdirectories. Avoid replacing files in SYSTEM just to install an unrelated program. Appendix B provides a fuller directory map.")]

#heading(level: 2)[#text("Adding files from another computer")]#label("h-adding-files-from-another-computer")

#procedure[#text("To copy files directly to the system card")]

#items[#enum(start: 1, [#text("Save your work and run ")#inline-code("POWEROFF")#text(".")], [#text("After shutdown, remove the card and connect it to another computer.")], [#text("Open the FAT32 partition labelled PI-DOS.")], [#text("Copy your files into appropriate directories such as GAMES or DOCS.")], [#text("Eject the card safely, reinstall it and start Pi-DOS.")])]

#p[#text("The root of PI-DOS becomes the root of C:. Do not add an extra ")#inline-code("c")#text(" directory around all your files. Leave the BOOT files and PI-DOS volume label intact.")]

#heading(level: 2)[#text("Copying from USB storage")]#label("h-copying-from-usb-storage")

#p[#text("Eligible FAT or exFAT USB volumes receive free letters starting at E:. Letters can change after a reboot or when devices are removed, so inspect the mapping each time.")]

#procedure[#text("To copy a document from USB")]

#items[#enum(start: 1, [#text("Connect the USB device and type ")#inline-code("MOUNT")#text(".")], [#text("Confirm its assigned letter and inspect its contents with ")#inline-code("DIR E:")#text("; substitute the actual letter.")], [#text("Copy the intended file, for example:")])]

#code-block("COPY E:\\​LETTER.TXT C:\\​DOCS\\​LETTER.TXT")

#p[#text("Wait for successful completion before removing the device. Close files in editors or applications first. You can optionally release an idle USB drive with ")#inline-code("MOUNT -U E")#text("; that connection remains ejected until it is physically reconnected.")]

#p[#text("COPY is a file-copy command, not a whole-directory backup recipe. Do not assume a command copying ")#inline-code("*.*")#text(" includes every nested directory or hidden file. Use the file manager or a suitable backup tool and verify the result.")]

#heading(level: 2)[#text("Using Doszip Commander")]#label("h-using-doszip-commander")

#p[#text("Type ")#inline-code("DZ")#text(" to open the supplied two-panel file manager. Use its displayed controls and help to browse, copy and organize files. Its settings live beside the program under ")#inline-code("C:\\PROGRAMS\\DOSZIP")#text(".")]

#p[#text("Before copying or deleting, check the source panel, destination panel and selected files. A two-panel view helps compare locations, but it does not prevent overwriting a newer file with an older one.")]

#heading(level: 2)[#text("Running a program")]#label("h-running-a-program")

#procedure[#text("To start an installed DOS program")]

#items[#enum(start: 1, [#text("Select the drive containing the program.")], [#text("Change to the program's directory.")], [#text("Type the program's executable name.")])]

#p(sticky: true)[#text("For example, if you have supplied a compatible program named APP.EXE:")]

#code-block("C:\nCD \\​PROGRAMS\\​MYAPP\nAPP")

#p[#text("Follow the program's own installation instructions. Some software expects to run from its installation directory; other software needs a particular CPU, memory, video adapter or sound setup. Chapters 5 through 7 explain those choices.")]

#p[#text("Pi-DOS runs compatible COM, MZ EXE and BAT files. A modern Windows executable is not a DOS program. Windows 3.x is a separate, user-supplied installation path, not an application automatically included with Pi-DOS.")]

#heading(level: 2)[#text("Editing text and keeping backups")]#label("h-editing-text-and-keeping-backups")

#p(sticky: true)[#text("Use ")#inline-code("EDIT")#text(" for plain-text files such as your startup batch file. Before a significant edit, copy the existing file to a distinct backup name. For example:")]

#code-block("COPY C:\\​AUTOEXEC.BAT C:\\​AUTOEXEC.OLD\nEDIT C:\\​AUTOEXEC.BAT")

#p[#text("Choose a backup name that does not overwrite a copy you still need. Setup has its own one-generation configuration backup, but it is not a backup of your documents or games.")]

#heading(level: 2)[#text("File attributes and open files")]#label("h-file-attributes-and-open-files")

#p(sticky: true)[#text("DOS attributes identify read-only, hidden, system and archive-marked files. Inspect a file with:")]

#code-block("ATTRIB C:\\​DOCS\\​LETTER.TXT")

#p[#text("To mark that file read-only, type ")#inline-code("ATTRIB +R C:\\DOCS\\LETTER.TXT")#text(". To allow normal modification again, type ")#inline-code("ATTRIB -R C:\\DOCS\\LETTER.TXT")#text(".")]

#p[#text("New writable opens, replacement and deletion of read-only files are rejected. A program that already holds a writable handle retains that access. Hidden and system files may be absent from an ordinary listing; ")#inline-code("DIR /A")#text(" requests attribute-aware listing.")]

#p[#text("Pi-DOS also enforces DOS file sharing and record locking. If a copy, rename or deletion reports a sharing or lock violation, close the program using the file before retrying. Do not disable sharing as the first response to a legitimate conflict.")]

#heading(level: 2)[#text("Backing up your work")]#label("h-backing-up-your-work")

#procedure[#text("To make a complete user-data backup")]

#items[#enum(start: 1, [#text("Save all applications and run ")#inline-code("POWEROFF")#text(".")], [#text("Open the PI-DOS partition on another computer.")], [#text("Copy its complete contents, including hidden files and directories, to a separate backup location.")], [#text("Verify that important documents, game saves and SYSTEM configuration files are present.")], [#text("If you use ZIP drives, verify that each archive's matching ")#inline-code(".PFS")#text(" directory was copied too.")])]

#p[#text("Keep an independent backup before updates, migration or filesystem repair. Copy BOOT as well, or retain the matching original image, when you need to preserve the full boot setup. A backup stored only on the same card does not protect against card failure.")]

#heading(level: 2)[#text("Checking space and names")]#label("h-checking-space-and-names")

#p[#text("Use directory listings to review storage and remove only files you recognize. Do not treat ordinary USB data mappings as raw DOS hard disks: FDISK, raw disk formatting and DOS defragmenters are not the maintenance route for those mappings.")]

#p[#text("FAT32 has a per-file size limit below 4 GiB. Keep that in mind when storing large images on C:. USB exFAT is a supported data-volume format, but an older DOS application may have smaller file-size or free-space limits of its own.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 4 Configuring Your System")]#label("h-chapter-4-configuring-your-system")

#p[#text("Pi-DOS separates the computer's saved hardware settings from the commands you want to run at startup. Use PIDOSCFG for supported setup controls, PIDOS.CFG for advanced settings, and AUTOEXEC.BAT for your own startup commands.")]

#heading(level: 2)[#text("Opening setup")]#label("h-opening-setup")

#p(sticky: true)[#text("Type:")]

#code-block("PIDOSCFG")

#p[#text("Setup has eight pages: CPU, Memory, Video, Sound, Mixer, Input, Network and System. The System page scrolls to additional fields. Follow its arrow or More indicators to reach them.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Key")],
[#text("Operation")],
[#text("F5 / F6")],
[#text("Previous / next page")],
[#text("Tab / Shift+Tab")],
[#text("Next / previous control")],
[#text("Enter")],
[#text("Edit or activate the focused control")],
[#text("Arrow keys")],
[#text("Navigate choices")],
[#text("Home / End, PgUp / PgDn")],
[#text("Move within lists")],
[#text("F2 or Alt+S")],
[#text("Save edited settings across all pages")],
[#text("F3 or Alt+R")],
[#text("Restore defaults on the current page, pending Save")],
[#text("Esc")],
[#text("Cancel without saving ordinary settings")],
), compact: false)

#p[#text("Some lists support searching by typing. Mouse selection is optional. F3 changes the current page only and does not take effect on disk until you save.")]

#procedure[#text("To change a setting")]

#items[#enum(start: 1, [#text("Run ")#inline-code("PIDOSCFG")#text(" and move to the relevant page.")], [#text("Select the control and choose the new value.")], [#text("Review any related settings on other pages.")], [#text("Press ")#strong[#text("F2")]#text(" to save.")], [#text("Return to the prompt and run ")#inline-code("REBOOT")#text(" to apply the saved configuration.")])]

#p[#text("Hardware, display, input, network and regional changes normally require rebooting. The live mixer is the documented exception: eligible channels preview immediately. Chapter 7 explains when mixer changes are live and how Cancel restores them.")]

#heading(level: 2)[#text("Understanding PIDOS.CFG")]#label("h-understanding-pidoscfg")

#p(sticky: true)[#text("The authoritative user configuration is:")]

#code-block("C:\\​SYSTEM\\​PIDOS.CFG")

#p[#text("The file contains INI-style sections. Pi-DOS-specific settings appear in ")#inline-code("[pidos]")#text("; other sections configure the DOS runtime. On each boot, Pi-DOS reads the saved file and generates the configuration it needs internally.")]

#p[#text("Existing settings are preserved. A new image or newly added default does not necessarily replace a choice saved on an older installation. Use the current setup controls or restore defaults on the relevant page when you deliberately want the current defaults.")]

#procedure[#text("To edit an advanced setting")]

#items[#enum(start: 1, [#text("Make an independent copy of PIDOS.CFG before editing.")], [#text("Run ")#inline-code("EDIT C:\\SYSTEM\\PIDOS.CFG")#text(".")], [#text("Change only settings whose effects you understand.")], [#text("Save and exit, then run ")#inline-code("REBOOT")#text(".")])]

#p[#text("Prefer setup for its supported controls. A valid-looking upstream runtime option may conflict with Pi-DOS's managed hardware and drive policies. Selecting a managed monitor profile, for example, makes that profile authoritative over low-level display choices.")]

#heading(level: 2)[#text("Setup backups and interrupted saves")]#label("h-setup-backups-and-interrupted-saves")

#p[#text("When setup saves, it writes a complete PIDOS.TMP first. It then keeps the previous configuration as PIDOS.BAK and installs the new PIDOS.CFG. Each successful save replaces the previous backup; this is a single generation, not a history.")]

#p[#text("Setup preserves unrelated settings and separate-line comments while updating edited keys. It reports read-only files, invalid values, configuration files over 24 KiB and storage failures. It does not overwrite an existing temporary recovery file.")]

#p[#text("If a save fails or power was interrupted, inspect PIDOS.CFG, PIDOS.BAK and PIDOS.TMP before deleting or renaming anything. Use another computer if C: is not writable. Chapter 12 explains recovery.")]

#heading(level: 2)[#text("Customizing user startup")]#label("h-customizing-user-startup")

#p(sticky: true)[#text("Your startup file is:")]

#code-block("C:\\​AUTOEXEC.BAT")

#p[#text("Pi-DOS runs it after standard system initialization and first-boot setup. Add personal environment variables, PATH extensions or commands here. Do not duplicate the system's automatic drive registration or hardware initialization.")]

#procedure[#text("To add a tools directory to PATH")]

#items[#enum(start: 1, [#text("Create your tools directory and place compatible executables in it.")], [#text("Back up AUTOEXEC.BAT.")], [#text("Add a line such as the following, then save:")])]

#code-block("SET PATH=%PATH%;​C:\\​PROGRAMS\\​MYTOOLS")

#items[#enum(start: 4, [#text("Reboot and check the resulting PATH at the prompt.")])]

#p[#text("The normal PATH already includes the writable editor/tools directory, SYSTEM and several Z: utility directories. Replacing it with only your new directory can make standard commands harder to find.")]

#p[#text("TEMP and TMP normally point to ")#inline-code("C:\\TEMP")#text(" on writable boots. Leave a writable temporary location available for setup's live-control queries and applications that need working files.")]

#heading(level: 2)[#text("CONFIG.SYS and familiar MS-DOS advice")]#label("h-configsys-and-familiar-ms-dos-advice")

#p[#text("Pi-DOS uses its internal DOS environment and native startup system. Copying a PC's CONFIG.SYS does not replace the Pi's boot process. Driver loading and CONFIG.SYS compatibility are limited to what the runtime implements.")]

#p[#text("Do not copy memory-manager, disk-compression or startup recipes from MS-DOS 6 without checking that they apply. This guide does not prescribe HIMEM, EMM386, DoubleSpace or MS-DOS system utilities as Pi-DOS requirements.")]

#p[#text("A separately booted DOS image has its own startup rules and needs separate validation. The instructions in this guide assume the standard Pi-DOS environment unless stated otherwise.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 5 Choosing Processor Speed and Memory")]#label("h-chapter-5-choosing-processor-speed-and-memory")

#p[#text("The CPU and Memory pages describe the emulated PC, not the physical Raspberry Pi. Changing a CPU profile changes the processor model and execution speed presented to DOS software; it does not change the Pi's physical clock.")]

#heading(level: 2)[#text("Selecting a CPU profile")]#label("h-selecting-a-cpu-profile")

#p[#text("The fresh-image default is ")#strong[#text("Pentium 120 MHz")]#text(". The MHz labels indicate approximate performance classes. They do not guarantee the speed, timing or benchmark score of a particular original PC.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Profile ID")],
[#text("Displayed class")],
[#inline-code("auto")],
[#text("Automatic")],
[#inline-code("286-12")],
[#text("286 12 MHz")],
[#inline-code("386-33")],
[#text("386DX 33 MHz")],
[#inline-code("486-33")],
[#text("486DX 33 MHz")],
[#inline-code("486-66")],
[#text("486DX2 66 MHz")],
[#inline-code("486-100")],
[#text("486DX4 100 MHz")],
[#inline-code("pentium-75")],
[#text("Pentium 75 MHz")],
[#inline-code("pentium-100")],
[#text("Pentium 100 MHz")],
[#inline-code("pentium-120")],
[#text("Pentium 120 MHz, default")],
[#inline-code("mmx-166")],
[#text("Pentium MMX 166 MHz")],
), compact: false)

#procedure[#text("To try a slower PC profile")]

#items[#enum(start: 1, [#text("Open ")#inline-code("PIDOSCFG")#text(" and select CPU.")], [#text("Choose a fixed profile appropriate to the program, such as 486DX2 66 MHz.")], [#text("Save and run ")#inline-code("REBOOT")#text(".")], [#text("Start the program again and compare its behavior.")])]

#p[#text("You can list profile choices from the command line with ")#inline-code("PIDOSCFG /LIST")#text(". For example, ")#inline-code("PIDOSCFG /PROFILE 486-66")#text(" selects that saved profile; reboot afterward. Profile selection does not independently change memory or FPU settings.")]

#p[#text("Use one change at a time when diagnosing timing problems. A slow menu, fast animation or music problem can depend on application timing rather than raw processor throughput.")]

#heading(level: 2)[#text("Fixed speed and Automatic")]#label("h-fixed-speed-and-automatic")

#p[#text("Fixed profiles request a particular execution budget. Automatic adapts execution speed, which can help some workloads but change timing behavior in others. A Pi still needs enough sustained performance to meet the requested budget.")]

#p[#text("The technical reference records a MIDI rhythm or stuttering issue in tested Sierra software, including Leisure Suit Larry 6, with Automatic. Tested fixed profiles avoided it. Use a fixed profile for that case; disabling presentation VSync did not solve it and introduced tearing.")]

#p[#text("These are global settings. Pi-DOS does not supply per-game hardware profiles or automatic game-specific tuning. Record a working configuration for programs that need special settings.")]

#heading(level: 2)[#text("Choosing DOS memory")]#label("h-choosing-dos-memory")

#p[#text("The Memory page offers 1, 2, 4, 8, 16, 32, 64 and 128 MiB. The default is ")#strong[#text("16 MiB")]#text(". This is separate from the physical RAM reported for the Raspberry Pi.")]

#procedure[#text("To change the memory size")]

#items[#enum(start: 1, [#text("Open the Memory page in PIDOSCFG.")], [#text("Choose a size supported by the selected CPU.")], [#text("Save and reboot.")], [#text("Run the application and check its own requirements and messages.")])]

#p[#text("More memory is not always better. Some older programs fail with more than 16 MiB; others require a smaller configuration. Setup rejects sizes outside the selected CPU's address range.")]

#heading(level: 2)[#text("Conventional and extended memory")]#label("h-conventional-and-extended-memory")

#p[#text("DOS software may refer to conventional memory, upper memory, HMA, XMS, EMS, DPMI or a DOS extender. These are different ways programs use memory and services in the emulated PC.")]

#p[#text("Pi-DOS inherits these facilities from its DOS engine. Availability depends on configuration and the program. A message about insufficient conventional memory is not necessarily solved by increasing the total emulated RAM.")]

#p[#text("Start with the program's own installation notes. Avoid adding PC memory-manager drivers merely because an older guide recommends them for MS-DOS. An application may need its supplied DOS extender, and Pi-DOS does not provide every licensed extender or Windows component.")]

#heading(level: 2)[#text("Checking compatibility methodically")]#label("h-checking-compatibility-methodically")

#p[#text("Use the default configuration first unless the program specifies otherwise. If it fails, note the message, try the CPU family and memory size it expects, then check video and sound requirements.")]

#p[#text("Self-modifying code, copy protection, unusual drivers and timing-sensitive demos can need individual testing. Pi-DOS aims for compatibility; it does not promise cycle-accurate reproduction of every PC.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 6 Setting Up Your Display")]#label("h-chapter-6-setting-up-your-display")

#p[#text("There are two separate display choices: the video adapter seen by DOS software and the monitor profile used to present its image on your physical screen. Choose both to suit your equipment and application.")]

#heading(level: 2)[#text("Choosing an emulated video adapter")]#label("h-choosing-an-emulated-video-adapter")

#p[#text("The default adapter is ")#strong[#text("S3 Trio64")]#text(". The Video page also offers CGA, Hercules, Tandy, EGA, VGA, Tseng ET4000 and VESA VBE 2.0/3.0 choices.")]

#p[#text("An application may require a particular adapter or mode. Select the adapter before starting that application. Choosing an older adapter can help reproduce its intended graphics, but may make newer high-resolution modes unavailable.")]

#p[#text("Software Voodoo emulation is enabled by default with maximum memory. It emulates 3dfx hardware in software; it does not mean the Pi contains a Voodoo card or that every accelerated game will be fast. Host Glide passthrough is disabled in the baseline.")]

#heading(level: 2)[#text("Using a CRT monitor")]#label("h-using-a-crt-monitor")

#p[#text("CRT is the default monitor profile. It follows the guest's rendered dimensions and attempts to select a corresponding host mode near the guest refresh rate. Standard VGA text begins at 720 by 400 pixels at roughly 70 Hz.")]

#p[#text("Use a display and adapter that can accept the selected timing. A listed mode is not proof that a physical monitor will lock onto it. Pi-DOS supplies VGA-era display timings, not original Hercules electrical output.")]

#p[#text("CRT mode does not add artificial scanlines, phosphor effects or shadow masks. Its intended appearance comes from a physical CRT. It also avoids software aspect correction; physical monitor geometry determines the final shape.")]

#heading(level: 2)[#text("Using a flat-panel display")]#label("h-using-a-flat-panel-display")

#p[#text("LCD holds a selected host resolution while DOS changes video modes. Available choices are 640x480, 800x600, 832x624, 1024x768, 1280x720 and 1920x1080. The retained LCD default is 1024x768.")]

#procedure[#text("To configure an LCD")]

#items[#enum(start: 1, [#text("In PIDOSCFG, select the LCD monitor profile.")], [#text("Choose a resolution your display accepts.")], [#text("Select ")#strong[#text("4:3")]#text(" or ")#strong[#text("Original pixels")]#text(" for the image shape.")], [#text("Leave linear filtering off for sharper pixel edges, or enable it for a smoother scaled image.")], [#text("Save and reboot.")])]

#p[#text("LCD mode fits the image with centered borders and reduces oversized guest modes to fit. Original pixels preserves the rendered raster ratio: for example, 320x200 is 8:5. A 4:3 choice deliberately changes that displayed proportion.")]

#p[#text("There is no automatic native-panel detection, integer-scale control or user refresh selector in the described setup. LCD preferences remain saved while CRT is selected.")]

#heading(level: 2)[#text("Inspecting available modes")]#label("h-inspecting-available-modes")

#p(sticky: true)[#text("Use:")]

#code-block("VIDEOMOD /P\nVIDEOMOD /HOST\nVIDEOMOD /DOS")

#p[#inline-code("/P")#text(" pages the output. ")#inline-code("/HOST")#text(" lists advertised physical display modes; ")#inline-code("/DOS")#text(" lists DOS BIOS/VESA modes. ")#inline-code("VIDEOMOD /?")#text(" shows help.")]

#p[#text("Pi-DOS enables presentation VSync. That controls presentation timing and is separate from the engine's emulated VGA VSync setting. A guest/host refresh mismatch may still cause repeated or dropped frames.")]

#heading(level: 2)[#text("If the display is blank or incorrectly shaped")]#label("h-if-the-display-is-blank-or-incorrectly-shaped")

#p[#text("Check power, cable, connector and the display's supported modes first. If the saved CRT profile selects a timing your display cannot accept, review the saved monitor settings from another computer or use a compatible display to select LCD.")]

#p[#text("For more detail, enable diagnostics and inspect VIDEO.LOG after the next boot. It records requested guest dimensions and refresh, the rendered dimensions and the selected host mode. Chapter 12 describes collecting logs before they are replaced.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 7 Setting Up Sound and Music")]#label("h-chapter-7-setting-up-sound-and-music")

#p[#text("A game's sound setup and Pi-DOS's physical audio output are different choices. The game addresses an emulated PC sound device; Pi-DOS mixes that sound and sends it to HDMI, the analogue jack or a USB DAC.")]

#heading(level: 2)[#text("Choosing a PC sound device")]#label("h-choosing-a-pc-sound-device")

#p[#text("The default is ")#strong[#text("Sound Blaster Pro 2")]#text(", with base address ")#strong[#text("220h")]#text(", ")#strong[#text("IRQ 7")]#text(" and ")#strong[#text("DMA 1")]#text(". The PC speaker is independently enabled.")]

#p[#text("Other presets include None, Tandy, AdLib, Creative Game Blaster/CMS, Sound Blaster 2.0, Sound Blaster 16 and Gravis Ultrasound Classic. Choosing a preset enables that managed device and disables competing managed cards. MIDI remains independent.")]

#procedure[#text("To match a game's digital sound setup")]

#items[#enum(start: 1, [#text("In PIDOSCFG, choose the sound device and resources you want.")], [#text("Save and reboot.")], [#text("Run the game's sound setup.")], [#text("Select the same device, address, IRQ and DMA values.")], [#text("Run the game's sound test if provided, then save its settings.")])]

#p[#text("At the DOS prompt, ")#inline-code("SET BLASTER")#text(" shows the current sound environment. Do not select SB16 in a game while Pi-DOS is still configured for SB Pro 2 merely because the game offers that option.")]

#p[#text("GUS software may need user-supplied compatible patch files under the configured ")#inline-code("C:\\ULTRASND\\MIDI")#text(" location. Those patches are not bundled as a general guarantee.")]

#heading(level: 2)[#text("Choosing the physical output")]#label("h-choosing-the-physical-output")

#p[#text("The System page selects HDMI, 3.5 mm jack or external USB DAC. Save and reboot after changing it.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Output")],
[#text("What to check")],
[#text("HDMI")],
[#text("Default; HDMI0 on dual-HDMI boards")],
[#text("3.5 mm jack")],
[#text("A board with the socket and enabled analogue support")],
[#text("USB DAC")],
[#text("A compatible playback-capable USB audio-class device connected before boot")],
), compact: false)

#p[#text("Pi-DOS waits briefly for USB audio discovery. If the selected output is missing, that boot remains silent; it does not silently choose another output. With multiple USB DACs, selection follows ALSA card/device order rather than a user-selected stable device identity.")]

#p[#text("All internally mixed audio, internal MIDI and extracted CD audio follows the selected output. External MIDI module audio is separate.")]

#heading(level: 2)[#text("Choosing a MIDI profile")]#label("h-choosing-a-midi-profile")

#reference-table((0.85fr, 1.15fr), (
[#text("Profile")],
[#text("Required sound source")],
[#text("None")],
[#text("MIDI disabled")],
[#text("Roland SC-55")],
[#inline-code("C:\\SYSTEM\\MIDI\\SC55.SF2")],
[#text("Roland GM")],
[#inline-code("C:\\SYSTEM\\MIDI\\GMROLAND.SF2")],
[#text("Roland MT-32")],
[#text("Matching control and PCM ROMs in ")#inline-code("C:\\SYSTEM\\MIDI\\MT32")],
[#text("Roland CM-32L")],
[#text("Matching ROMs in ")#inline-code("C:\\SYSTEM\\MIDI\\CM32L")],
[#text("External USB MIDI")],
[#text("A connected, selected MIDI interface/module")],
), compact: false)

#p[#text("SC-55 is the fresh-image profile, but it needs the corresponding SoundFont. The SC-55 and Roland GM choices use SoundFont synthesis; they are not verified hardware-accurate emulations of those sound modules. MT-32 and CM-32L use Munt with their respective ROM pairs.")]

#p[#text("Assets must be supplied through the project's authorized asset workflow and match its manifest. They are not included in a fresh source clone. Missing assets or a missing device are reported in the boot MIDI row and disable that managed MIDI profile for the boot.")]

#p[#text("Choose matching music hardware in the game's own setup. Sound Blaster digital effects and internal MIDI music can play together.")]

#heading(level: 2)[#text("Connecting an external MIDI module")]#label("h-connecting-an-external-midi-module")

#procedure[#text("To select an external MIDI destination")]

#items[#enum(start: 1, [#text("Connect the USB MIDI interface and turn on the module before boot.")], [#text("In PIDOSCFG, choose External USB MIDI and select the destination.")], [#text("Use ")#strong[#text("F5")]#text(" in the destination picker if you need to rescan.")], [#text("Save and reboot.")], [#text("In the game, select the music hardware expected by your module.")])]

#p[#text("Pi-DOS saves the destination by stable identity. For devices without a unique serial number, keep the interface on the same USB port. A missing or ambiguous destination is not replaced by another module.")]

#p[#text("Connect the module's audio outputs to its own speakers or mixer. MIDI carries performance messages, not the module's resulting audio. The internal MIDI mixer control cannot change the volume of an external module's analogue output.")]

#heading(level: 2)[#text("Adjusting the mixer")]#label("h-adjusting-the-mixer")

#p[#text("The Mixer page provides master and device-specific levels. Values range from 0 to 200 percent for left and right channels. Levels above 100 percent can clip.")]

#procedure[#text("To preview and save mixer levels")]

#items[#enum(start: 1, [#text("Start with the intended sound hardware already active.")], [#text("Open PIDOSCFG and select Mixer.")], [#text("Adjust an enabled channel marked ")#strong[#text("LIVE")]#text(".")], [#text("Press ")#strong[#text("F2")]#text(" to keep the preview and save it, or cancel to restore the starting levels.")])]

#p[#text("Pending hardware changes suspend live preview and restore original levels. Inactive channels or unavailable live control show ")#strong[#text("AFTER REBOOT")]#text(". Temporary queries need writable C: storage.")]

#p[#text("If music stutters with Automatic CPU, try a fixed profile as described in Chapter 5. If there is no sound at all, check the physical output, game resources, mixer levels and MIDI assets separately.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 8 Using Disks and Removable Media")]#label("h-chapter-8-using-disks-and-removable-media")

#p[#text("Pi-DOS manages its main drive letters and discovers supported removable devices. Physical devices, disk images and ZIP drives have different rules; choose the right method for the media you are using.")]

#heading(level: 2)[#text("Understanding drive assignments")]#label("h-understanding-drive-assignments")

#reference-table((0.85fr, 1.15fr), (
[#text("Letter")],
[#text("Normal assignment")],
[#text("A:")],
[#text("Automatic USB floppy")],
[#text("C:")],
[#text("Persistent system/data partition, DOS label SYSTEM")],
[#text("D:")],
[#text("Automatic USB optical drive")],
[#text("E: onward")],
[#text("Eligible USB data volumes or free-letter mounts")],
[#text("Z:")],
[#text("Internal runtime utilities, DOS label PI-DOS")],
), compact: false)

#p[#text("C: and Z: are protected. You cannot replace or relocate them in appliance mode, even when C: is unavailable. Arbitrary host-directory mounts, ordinary directory overlays and SUBST mappings are disabled.")]

#p[#text("Type ")#inline-code("MOUNT")#text(" to list mappings. Free a removable mapping before assigning that letter to an image. Automatic USB discovery skips letters already occupied by an explicit mount.")]

#heading(level: 2)[#text("Using USB sticks and external disks")]#label("h-using-usb-sticks-and-external-disks")

#p[#text("Pi-DOS automatically recognizes eligible FAT12, FAT16, FAT32 and exFAT USB data volumes. It supports ordinary USB sticks, hard disks, SSDs and card readers, including appropriate MBR and GPT partitions.")]

#p[#text("Volumes receive the lowest free letter starting at E:. Connected volumes retain their letters, but assignments are not saved across reboot. The system's own disk is excluded from USB data discovery.")]

#procedure[#text("To use a USB data volume")]

#items[#enum(start: 1, [#text("Connect the device and allow a moment for discovery.")], [#text("Type ")#inline-code("MOUNT")#text(" and identify its drive letter.")], [#text("Use DIR and ordinary file commands on that letter.")], [#text("Close files and wait for successful writes before removal.")])]

#p[#text("For optional ejection, type ")#inline-code("MOUNT -U E")#text(", substituting the correct letter. Close DOS files first. Reconnect the physical device before using that ejected connection again.")]

#p[#text("USB data mapping provides file access, not a raw emulated DOS hard disk. Pi-DOS does not automatically repair, repartition or format these volumes. Unsupported, hidden, reserved, conflicting or already mounted partitions can be skipped.")]

#heading(level: 2)[#text("Using a USB floppy drive")]#label("h-using-a-usb-floppy-drive")

#p[#text("A: automatically discovers suitable USB floppy hardware. Standard supported capacities span 320 KiB through 2.88 MiB with 512-byte sectors and valid FAT12 for file access; the drive must support the actual disk capacity.")]

#procedure[#text("To copy a file from floppy")]

#items[#enum(start: 1, [#text("Connect the drive and insert the disk.")], [#text("Type ")#inline-code("DIR A:")#text(" to inspect it.")], [#text("Copy the file, for example:")])]

#code-block("COPY A:\\​LETTER.TXT C:\\​DOCS\\​LETTER.TXT")

#p[#text("You can attach a drive before or after boot. After a disk change, Pi-DOS resets that drive's current directory and invalidates old handles. Close files before changing disks. Wait for command completion and the drive light to go out before removal.")]

#p[#text("If write-protect status cannot be reported by the drive, Pi-DOS uses it read-only. A USB stick is not automatically treated as a floppy drive.")]

#heading(level: 2)[#text("Formatting a floppy")]#label("h-formatting-a-floppy")

#notice[#text("")#strong[#text("Important")]#text(" FORMAT destroys the selected floppy's existing contents. Confirm A: and the inserted disk. Do not use these instructions on a disk containing the only copy of your files.")]

#procedure[#text("To quick-format an already recognizable floppy")]

#items[#enum(start: 1, [#text("Verify that the disk is expendable and not write-protected.")], [#text("Type ")#inline-code("FORMAT A: /Q")#text(".")], [#text("Read and answer FORMAT's prompts, then wait for completion.")])]

#p[#text("A full format can be requested with ")#inline-code("FORMAT A: /U")#text(". Quick formatting needs a recognizable existing format. Full formatting fills and verifies addressable sectors and writes FAT12 at the existing capacity; it cannot create missing physical tracks, change recording density or reproduce copy-protected formats.")]

#heading(level: 2)[#text("Using physical CDs")]#label("h-using-physical-cds")

#p[#text("D: automatically represents the first eligible USB optical drive. The integration targets ordinary data, digital-audio and mixed-mode CDs. DVD/Blu-ray drive hardware may be recognized, but DVD/Blu-ray disc reading and burning are outside this integration's scope.")]

#procedure[#text("To use a data CD")]

#items[#enum(start: 1, [#text("Connect the drive and insert the disc.")], [#text("Type ")#inline-code("DIR D:")#text(".")], [#text("Run the disc's DOS installer or copy permitted files according to its instructions.")])]

#p[#text("No manual mount or reboot is needed for a physical disc swap. Close files first. An absent or empty drive reports not ready. Only one physical optical drive receives the automatic D: mapping.")]

#p[#text("CD audio is extracted digitally and mixed through the selected physical audio output. It does not use a drive's analogue cable. Physical mixed-mode, CD-audio and hotplug acceptance are still pending in the technical baseline; try your actual drive and discs before depending on them.")]

#heading(level: 2)[#text("Mounting a CD image")]#label("h-mounting-a-cd-image")

#p[#text("Store your image files in a directory such as ")#inline-code("C:\\IMAGES")#text(". Create that directory if needed and copy the files there from another computer or removable media.")]

#procedure[#text("To use an ISO image on D:")]

#items[#enum(start: 1, [#text("Close programs using D:.")], [#text("Release the automatic optical mapping.")], [#text("Mount the intended image:")])]

#code-block("MOUNT -U D\nIMGMOUNT D C:\\​IMAGES\\​GAME.ISO -t iso")

#p[#text("The image must exist at that path. For mixed-mode CUE/BIN sets, use the CUE description with its associated BIN files, following the pinned engine's supported formats.")]

#p[#text("To restore automatic physical optical discovery, release the image mapping and run ")#inline-code("Z:\\SYSTEM\\PICDROM.COM")#text(" with D: free, or reboot. This restores the automatic backend; it does not convert an image into physical media.")]

#heading(level: 2)[#text("Using more than one image")]#label("h-using-more-than-one-image")

#procedure[#text("To load a two-disc CD set")]

#items[#enum(start: 1, [#text("Place both images in the intended directory.")], [#text("Release D: and mount the full set together:")])]

#code-block("MOUNT -U D\nIMGMOUNT D C:\\​IMAGES\\​DISC1.ISO C:\\​IMAGES\\​DISC2.ISO -t iso\nIMGSWAP D 2")

#p[#inline-code("IMGSWAP D 2")#text(" selects the second image. The default ")#strong[#text("Ctrl+Alt+F4")]#text(" shortcut advances and wraps mounted multi-image CD sets. It affects all such CD image drives, not physical discs or single-image mounts.")]

#p[#text("For floppy image sets, release A: and mount the image set with ")#inline-code("-t floppy")#text(". The default ")#strong[#text("Ctrl+Alt+F5")]#text(" shortcut cycles multi-image floppy sets. Image geometry and format must be supported by the engine.")]

#p[#text("There are no saved disc playlists, automatic disc selection or in-game selection menu. Keep a note or appropriate startup commands for the image sets you use. Physical in-game switching remains a validation item.")]

#heading(level: 2)[#text("Mounting a ZIP as a writable drive")]#label("h-mounting-a-zip-as-a-writable-drive")

#p[#text("ZIPMOUNT presents an archive as a DOS drive without changing the ZIP itself. Writes go into a sibling directory with the same basename and the extension ")#inline-code(".PFS")#text(".")]

#procedure[#text("To run a program from a ZIP drive")]

#items[#enum(start: 1, [#text("Choose a free drive letter; check MOUNT first. In this example E: must be free.")], [#text("Mount your archive and switch to that letter:")])]

#code-block("ZIPMOUNT E C:\\​GAMES\\​DOOM.ZIP\nE:")

#items[#enum(start: 3, [#text("Start the program as required by the archive's contents.")], [#text("Exit it and switch away from E: before unmounting:")])]

#code-block("C:\nZIPMOUNT E -u")

#p[#text("The example assumes you supplied the archive and that its contents are suitable. ZIPMOUNT is not an automatic game launcher or installer.")]

#heading(level: 2)[#text("Keeping ZIP saves")]#label("h-keeping-zip-saves")

#p[#text("For ")#inline-code("DOOM.ZIP")#text(", changes normally live in ")#inline-code("DOOM.PFS")#text(" beside it. Newly written files and modified copies take precedence over the archive. Deletion information is also retained so deleted archive entries remain hidden after remounting.")]

#p[#text("Copy ")#strong[#text("both the ZIP and its matching PFS directory")]#text(" when backing up or moving the program. Do not remove either while mounted. To reset to the archive's original content, unmount first and remove the overlay only after backing up any saves you want to keep.")]

#p(sticky: true)[#text("A read-only mount uses ")#inline-code("-ro")#text(":")]

#code-block("ZIPMOUNT E C:\\​GAMES\\​DOOM.ZIP -ro")

#p[#text("Read-only prevents new writes but still shows existing overlay changes. It does not mean a pristine view of the original ZIP. Failed overlay creation fails the mount; saves are not redirected to temporary RAM.")]

#heading(level: 2)[#text("Labels and media safety")]#label("h-labels-and-media-safety")

#p(sticky: true)[#text("Floppy and USB labels can be changed using LABEL, provided files are closed and the volume is writable. For example:")]

#code-block("LABEL E:USB GAMES\nVOL E:")

#p[#text("Labels are limited to 11 DOS bytes, uppercased using the guest code page. C: SYSTEM and Z: PI-DOS labels are protected; optical labels are read-only. Renaming the PI-DOS host partition from another computer can prevent startup from finding C:.")]

#p[#text("Successful writes are flushed before completion, but unplugging during a write can still lose data. Use clean shutdown for the system card and close files before ejecting removable devices.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 9 Using Input Devices and Shortcuts")]#label("h-chapter-9-using-input-devices-and-shortcuts")

#p[#text("Pi-DOS accepts supported wired USB keyboards, mice and controllers through the Raspberry Pi's input system. DOS applications then receive the emulated keyboard, mouse or gameport interfaces they expect.")]

#heading(level: 2)[#text("Keyboard and mouse")]#label("h-keyboard-and-mouse")

#p[#text("Choose the keyboard layout in setup independently of your DOS country and timezone. If letters or punctuation are wrong, change the keyboard layout first; country controls number/date conventions rather than the physical arrangement of keys.")]

#p[#text("A DOS application needs support for the emulated mouse interface to use a mouse. A connected physical mouse does not give every text program a mouse-driven interface. Keyboard and mouse hotplug are recorded as working in the supplied reference, while application-specific behavior can vary.")]

#heading(level: 2)[#text("Choosing a joystick or gamepad mode")]#label("h-choosing-a-joystick-or-gamepad-mode")

#p[#text("The default presents two conventional two-axis, two-button DOS joysticks. Other choices include four-axis configurations, Thrustmaster FCS and CH Flightstick modes. Select a mode the game understands.")]

#p[#text("Known controllers use SDL mappings in Automatic layout. Unknown devices can fall back to raw axes and buttons; Raw bypasses normalization. You can choose left stick with D-pad, stick only or D-pad only, swap the first two buttons, and set a dead zone.")]

#procedure[#text("To configure a controller")]

#items[#enum(start: 1, [#text("Connect the controller and open PIDOSCFG's Input page.")], [#text("Choose the DOS joystick type and layout appropriate to the game.")], [#text("Adjust the dead zone or button swap if needed.")], [#text("Save and reboot.")], [#text("Start the game and use its joystick calibration routine.")])]

#p[#text("Controllers can connect and reconnect after boot. Active devices keep their slots; a new device fills the first vacancy. Disconnection releases buttons and centers axes. A game that probes only at startup may need restarting after a reconnect.")]

#p[#text("Modern extra buttons are limited by the selected DOS joystick interface. There is no gamepad-to-keyboard mapper or per-game controller profile system. Physical calibration and gamepad hotplug still need acceptance for your particular hardware and game.")]

#heading(level: 2)[#text("System shortcut keys")]#label("h-system-shortcut-keys")

#reference-table((0.9fr, 1.1fr), (
[#text("Default shortcut")],
[#text("Action")],
[#text("Ctrl+Alt+Delete")],
[#text("Clean reboot of the Raspberry Pi")],
[#text("Ctrl+Alt+PrintScreen")],
[#text("Save a numbered PNG screenshot")],
[#text("Ctrl+Alt+F4")],
[#text("Next image in mounted CD image sets")],
[#text("Ctrl+Alt+F5")],
[#text("Next image in mounted floppy image sets")],
[#text("None assigned")],
[#text("Poweroff")],
[#text("None assigned")],
[#text("Pause/resume")],
), compact: false)

#p[#text("F12 remains the runtime Host key. The listed Pi-DOS shortcuts can be reassigned to None, Ctrl+Alt+Delete, Ctrl+Alt+F1 through F10, or Ctrl+Alt+PrintScreen. Setup rejects duplicate assignments.")]

#procedure[#text("To change a system shortcut")]

#items[#enum(start: 1, [#text("Open the System page and find the desired action; scroll if needed.")], [#text("Choose an unused key combination or None.")], [#text("Save and reboot.")], [#text("Check the assignment before using it in an application.")])]

#p[#text("A disabled shortcut returns to normal DOS input. Assigned combinations suppress repeated key events. Shift/GUI variants remain available to the guest.")]

#p[#text("If you assign Pause, press and release the shortcut to pause. Repeat it, or press Escape, to resume. Pausing is not a save state and does not write a game's progress to disk.")]

#heading(level: 2)[#text("Taking a screenshot")]#label("h-taking-a-screenshot")

#p[#text("Press ")#strong[#text("Ctrl+Alt+PrintScreen")]#text(" with the default assignment. Pi-DOS saves numbered PNG files in ")#inline-code("C:\\SYSTEM\\CAPTURES")#text(", using a name based on the program, such as ")#inline-code("doom_000.png")#text(".")]

#p[#text("The capture contains the DOS image before LCD scaling and borders. C: must be available and writable. If it is not, the screenshot is not silently stored elsewhere. Physical Pi capture acceptance remains open in the baseline, although dispatch and image-generation checks are recorded.")]

#p[#text("Pi-DOS does not supply save states, rewind or integrated video-recording workflows. Use an application's own save function for progress and documents.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 10 Setting the Clock, Region and Network")]#label("h-chapter-10-setting-the-clock-region-and-network")

#p[#text("Your keyboard, DOS country and timezone are independent settings. Networking also has two layers: the Raspberry Pi's background connection and the optional network adapter seen by DOS software.")]

#heading(level: 2)[#text("Changing regional settings")]#label("h-changing-regional-settings")

#procedure[#text("To select your local conventions")]

#items[#enum(start: 1, [#text("Open the System page in PIDOSCFG.")], [#text("Select the keyboard layout matching your keyboard.")], [#text("Select the DOS country and, when appropriate, code-page choice.")], [#text("Select a timezone from the installed catalog.")], [#text("Save and reboot.")])]

#p[#text("The baseline defaults are keyboard ")#inline-code("sp")#text(", country ")#inline-code("34")#text(" and timezone ")#inline-code("Europe/Madrid")#text(". Searchable lists help locate other choices. Timezone rules provide daylight-saving changes; receiving network time does not choose your timezone for you.")]

#heading(level: 2)[#text("Reviewing date and time")]#label("h-reviewing-date-and-time")

#p[#text("Use DATE and TIME to inspect or change the DOS clock. Follow their prompts if you choose to make a manual change.")]

#p(sticky: true)[#text("Manual changes can suspend host-to-DOS time synchronization. To resume synchronization, type:")]

#code-block("DATE /S")

#p[#text("The background host service obtains time over wired networking when it can. Startup does not wait for that service, and the displayed time may change after synchronization. Offline startup depends on an available working real-time clock.")]

#heading(level: 2)[#text("Host networking and guest networking")]#label("h-host-networking-and-guest-networking")

#reference-table((0.85fr, 1.15fr), (
[#text("Layer")],
[#text("Purpose")],
[#text("Raspberry Pi host connection")],
[#text("Wired DHCP, DNS and network time")],
[#text("Optional DOS connection")],
[#text("Networking used by DOS or Windows software")],
), compact: false)

#p[#text("The host can synchronize time even while DOS IPX and NE2000 networking are disabled. Both guest features are off by default. The appliance image has no Wi-Fi, Bluetooth, IPv6, SSH server or normal Linux login workflow.")]

#p[#text("You can use DOS offline. The current image still starts its host network services in the background, but unavailable networking does not prevent normal DOS startup.")]

#heading(level: 2)[#text("Enabling DOS TCP/IP with SLIRP")]#label("h-enabling-dos-tcpip-with-slirp")

#p[#text("SLIRP provides NAT-style guest connectivity through an emulated NE2000 adapter. It is the simpler starting point for DOS TCP/IP when direct attachment to the physical LAN is unnecessary.")]

#procedure[#text("To configure the bundled DOS TCP/IP tools")]

#items[#enum(start: 1, [#text("In PIDOSCFG's Network page, enable NE2000.")], [#text("Choose SLIRP and the intended DOS TCP/IP startup mode.")], [#text("Keep adapter resources consistent with the packet driver; the defaults are I/O 300h and IRQ 10.")], [#text("Choose DHCP or appropriate static guest settings.")], [#text("Save and reboot, then run ")#inline-code("NETINFO")#text(" to inspect settings and status.")])]

#p[#text("If startup did not complete, use ")#inline-code("NETUP")#text(" to retry. Retry is bounded so an absent lease or cable does not block DOS indefinitely.")]

#p[#text("The bundled mTCP tools include FTP, HTGET, TELNET, PING and DNSTEST. Their documentation, license and corresponding source are under ")#inline-code("Z:\\NETWORK\\MTCP")#text(". Use that manual for each tool's arguments.")]

#p[#text("Default SLIRP guest addressing is 10.0.2.15 with mask 255.255.255.0, gateway 10.0.2.2 and DNS 10.0.2.3. These are guest addresses, separate from the Pi's physical LAN address. Inbound services through NAT need appropriate TCP/UDP forwarding configuration.")]

#heading(level: 2)[#text("Preserving custom TCP/IP settings")]#label("h-preserving-custom-tcpip-settings")

#p[#text("Pi-DOS generates networking files under ")#inline-code("C:\\SYSTEM\\NET")#text(" each boot. ")#inline-code("MTCPCFG")#text(" normally points to the generated MTCP.CFG there; DHCP updates its lease data.")]

#p[#text("Do not treat edits to generated files as persistent application configuration. For custom mTCP options, select Manual startup and maintain a separate user configuration referenced by your own MTCPCFG setting.")]

#p(sticky: true)[#text("The full installed networking guide is available at:")]

#code-block("C:\\​DOCS\\​NETWORK.TXT\nZ:\\​NETWORK\\​NETWORK.TXT")

#heading(level: 2)[#text("Playing through an IPX tunnel")]#label("h-playing-through-an-ipx-tunnel")

#p[#text("IPX tunnel mode connects compatible emulator endpoints over UDP. One system acts as server and the others connect as clients. The default UDP port is 213.")]

#procedure[#text("To prepare an IPX tunnel session")]

#items[#enum(start: 1, [#text("Enable IPX in the Network page.")], [#text("Choose server mode on one system and client mode on the others.")], [#text("Set the server address and matching port as required.")], [#text("Save, reboot and check the connection using NETINFO and the installed networking guide.")], [#text("Start the games with compatible multiplayer settings.")])]

#p[#text("NETUP retries saved startup. IPXNET provides manual control and PING facilities. Firewalls must allow the chosen UDP traffic. A tunnel does not put native IPX frames onto a real retro-PC LAN.")]

#heading(level: 2)[#text("Connecting directly to a wired LAN")]#label("h-connecting-directly-to-a-wired-lan")

#p[#text("PCAP presents the guest's Ethernet frames on a selected wired host interface, usually ")#inline-code("eth0")#text(". The guest needs its own MAC/IP configuration, and the network must permit the additional MAC address. This is not Wi-Fi bridging.")]

#p[#text("For native IPX peers, use PCAP with the IPX tunnel disabled and supply compatible ODI components, including LSL.COM, ODI NE2000.COM and IPXODI.COM. The ODI driver is different from the bundled TCP/IP packet driver. Match I/O, IRQ and frame type. Some applications also need a compatible NETBIOS driver.")]

#p[#text("SLIRP is not a substitute for raw Ethernet protocols such as native IPX or NetBEUI. Follow NETWORK.TXT for the direct-LAN configuration rather than mixing drivers from different stacks.")]

#heading(level: 2)[#text("Windows for Workgroups")]#label("h-windows-for-workgroups")

#p[#text("Windows for Workgroups 3.11 is user-supplied. Its PCAP path uses its own NE2000 NDIS/protocol stack and may require a separately supplied Microsoft TCP/IP-32 package.")]

#p[#text("Match adapter resources, select Windows startup in PIDOSCFG, and run ")#strong[#text("WFWNET before WIN")]#text(". The helper loads the supplied installation's IFSHLP.SYS through DEVICE.COM and runs NET START. Do not first load DOS packet or ODI stacks for this path.")]

#p[#text("A separately booted DOS installation instead uses its own CONFIG.SYS and AUTOEXEC.BAT. Direct LAN access does not add SMB2/3 or modern authentication to Windows for Workgroups. Physical LAN, native-IPX and WfW sharing acceptance remain open in this baseline.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 11 Using the Supplied Tools")]#label("h-chapter-11-using-the-supplied-tools")

#p[#text("The bundled menus provide convenient starting points for benchmarks and demos. They preserve your selected hardware settings, so the menu itself does not make every item compatible with every configuration.")]

#heading(level: 2)[#text("Editing and file management")]#label("h-editing-and-file-management")

#p[#inline-code("EDIT")#text(" starts the writable personalized PEDIT Light editor. ")#inline-code("DZ")#text(" starts Doszip Commander. Their settings are stored with their writable program copies under PROGRAMS.")]

#p[#text("ARJ 2.41 is available at ")#inline-code("Z:\\BIN\\ARJ.EXE")#text(". Consult its included help and notices for archive operations. ZIPMOUNT, described in Chapter 8, mounts a ZIP as a DOS filesystem; it serves a different purpose from extracting an archive with an archiver.")]

#p[#text("No managed BASIC, Pascal, C or assembly development suite is installed as a product feature. You may supply compatible DOS development tools. The Open Watcom toolchain used to build setup is a development-host tool, not a bundled DOS IDE.")]

#heading(level: 2)[#text("Running benchmarks")]#label("h-running-benchmarks")

#p[#text("Type ")#inline-code("BENCH")#text(" to open the benchmark menu. Press ")#strong[#text("Q")]#text(" to return to the caller's drive and directory.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Menu choice")],
[#text("Benchmark")],
[#text("1")],
[#text("Superscape 3DBench 1.0c")],
[#text("2 / 3")],
[#text("Chris's 3D Benchmark, VGA / VESA")],
[#text("4 / 5")],
[#text("PC Player, VGA / VESA")],
[#text("6 / 7")],
[#text("Doom 1.9 shareware DEMO3, minimum / maximum detail")],
[#text("8 / 9 / A")],
[#text("Quake 1.06 shareware DEMO1, three video resolutions")],
), compact: false)

#p[#text("Doom and Quake benchmark sound is disabled. Quake needs an FPU, and high-resolution choices need a suitable VESA adapter. At the end of a Quake run, read the console FPS and use QUIT to return.")]

#procedure[#text("To compare two configurations")]

#items[#enum(start: 1, [#text("Record the Pi model, Pi-DOS version, CPU profile, RAM, video adapter and monitor settings.")], [#text("Run the same benchmark option and record the result.")], [#text("Change one setting, save and reboot.")], [#text("Repeat the same option under otherwise comparable conditions.")])]

#p[#text("A score measures the configured emulated environment. A CPU profile label does not guarantee an original PC's score. Cooling, throttling and the Pi's ability to sustain the requested emulation speed can affect results.")]

#heading(level: 2)[#text("Running demos")]#label("h-running-demos")

#p[#text("Type ")#inline-code("DEMOS")#text(" to open the numbered menu. Press ")#strong[#text("H")]#text(" for setup notes or ")#strong[#text("Q")]#text(" to return.")]

#p[#text("The pack contains Second Reality, Unreal, Panic, Crystal Dream I, Crystal Dream II, Stars: Wonders of the World, Caero, Inside and Fishtro. Inside opens its original setup first. The menu does not change CPU, memory, video or sound settings.")]

#p[#text("Start with the default 16 MiB and fixed CPU profile, then follow each production's original notes. Match SB/SB Pro resources to Pi-DOS. Tracker music uses the selected sound card, not the separate SC-55 MIDI profile.")]

#p[#text("Some productions require EMS or extended memory. If timing is wrong, try an appropriate fixed 486 profile. Archive and launch-file checks do not establish physical playback, music synchronization or speed for your hardware.")]

#heading(level: 2)[#text("Finding documentation and notices")]#label("h-finding-documentation-and-notices")

#p[#text("Look in C:\\DOCS, the application's directory and relevant Z: tool directories. Benchmark and demo directories retain original material and provenance files. Doszip retains its matching source archive and GPL notice.")]

#p[#text("An image update does not necessarily add newly introduced data packs to an existing C:. See Chapter 13 for updating user-data payloads without replacing unrelated SYSTEM files.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 12 Diagnosing and Solving Problems")]#label("h-chapter-12-diagnosing-and-solving-problems")

#p[#text("Begin with the smallest useful observation: the exact message, whether C: is available, what changed since the last successful boot, and whether the problem affects one program or the whole system.")]

#heading(level: 2)[#text("When the system reaches Z: instead of C:")]#label("h-when-the-system-reaches-z-instead-of-c")

#p[#text("A Z: recovery prompt means Pi-DOS could not use the expected data storage. There is no invented temporary C: hiding the problem. Common causes include a missing or unusable partition, an incorrect label, an older ext4 layout or filesystem errors.")]

#procedure[#text("To inspect the card safely")]

#items[#enum(start: 1, [#text("Avoid repeated configuration saves or other write attempts.")], [#text("Use a clean shutdown path where available. The default supervised reboot shortcut can operate without writable C:; the disk-based REBOOT batch path needs its request file.")], [#text("With the Pi off, connect the card to another computer.")], [#text("Check for the expected BOOT and FAT32 PI-DOS partitions.")], [#text("Back up readable data before changing labels or running that computer's filesystem-repair tools.")])]

#p[#text("Pi-DOS does not provide a general filesystem-repair tool or a Linux recovery shell. Do not reflash the card as the first response if it contains unbacked-up files.")]

#heading(level: 2)[#text("When C: is read-only")]#label("h-when-c-is-read-only")

#p[#text("A failed writable mount can fall back to read-only C:. Reading programs may still work, but setup, saves, logs and screenshots cannot persist. Pi-DOS does not redirect those writes to RAM.")]

#p[#text("Preserve readable files from another computer and investigate the card's filesystem, capacity and health. A read-only attribute on one file is different from an entirely read-only drive; ATTRIB cannot repair a read-only volume mount.")]

#heading(level: 2)[#text("When setup cannot save")]#label("h-when-setup-cannot-save")

#p[#text("Check whether C: is writable, PIDOS.CFG is read-only, the configuration exceeds 24 KiB, or a recovery PIDOS.TMP remains. Read the reported error before modifying files.")]

#procedure[#text("To recover an earlier configuration")]

#items[#enum(start: 1, [#text("Shut down and open PI-DOS on another computer if necessary.")], [#text("Preserve copies of PIDOS.CFG, PIDOS.BAK and PIDOS.TMP wherever present.")], [#text("Inspect their contents and identify a known-good complete configuration.")], [#text("Restore that copy as ")#inline-code("SYSTEM/PIDOS.CFG")#text(" without discarding your preserved originals.")], [#text("Eject the card safely and restart.")])]

#p[#text("The BAK file is the immediately previous successful configuration, not necessarily a factory default. Do not blindly delete TMP: it may contain the complete new file from an interrupted replacement.")]

#heading(level: 2)[#text("When the screen is black")]#label("h-when-the-screen-is-black")

#p[#text("Check display power and the physical connection. A CRT timing that the panel or adapter cannot accept can leave you with no usable picture. Try a known-compatible display or review the saved monitor configuration from another computer.")]

#p[#text("The current baseline includes a fix for a static black first-boot welcome after display initialization, but physical confirmation of that exact correction is pending. If the symptom persists, retain the version information and diagnostics rather than repeatedly rewriting data.")]

#heading(level: 2)[#text("When there is no sound")]#label("h-when-there-is-no-sound")

#p(sticky: true)[#text("Check these items separately:")]

#items[#list([#text("The selected physical output exists and is connected; HDMI audio defaults to HDMI0.")], [#text("The game selects the same sound card, address, IRQ and DMA as Pi-DOS.")], [#text("The relevant mixer channel is enabled and has an audible level.")], [#text("A MIDI profile has its required assets or external destination.")], [#text("External MIDI module audio is connected to speakers or a mixer.")])]

#p[#text("An unavailable selected jack or USB output does not trigger an automatic HDMI fallback. Missing MIDI assets can disable that profile even while digital sound effects work.")]

#p[#text("If Sierra MIDI timing is wrong with Automatic CPU, select a tested fixed profile. Keep presentation VSync enabled; disabling it did not resolve the recorded problem.")]

#heading(level: 2)[#text("When a removable drive is not ready")]#label("h-when-a-removable-drive-is-not-ready")

#p[#text("For A: or D:, confirm that compatible hardware is connected and media is inserted. For an image, confirm it was mounted on the letter you are using. For USB data, use MOUNT to check whether the assigned letter changed.")]

#p[#text("Close and reopen files after media changes. Pi-DOS deliberately invalidates stale handles so an application cannot write to a replacement disk using old state. Unsupported filesystems or reserved partitions may not appear as data drives.")]

#p[#text("A floppy may be read-only because of its write-protect tab or because the drive cannot report protection status. A physical optical disc is read-only by design.")]

#heading(level: 2)[#text("When files appear to be missing")]#label("h-when-files-appear-to-be-missing")

#p[#text("Check the current directory, drive letter and long/short-name view. Use ")#inline-code("DIR /A")#text(" when hidden or system attributes may be involved. USB letters are not persistent across reboot.")]

#p[#text("For a ZIP drive, confirm that the archive and matching PFS directory are both present. Existing overlay deletions remain effective even on a read-only mount. Do not delete the overlay to investigate until you have backed it up and unmounted it.")]

#heading(level: 2)[#text("When a program reports a sharing or lock error")]#label("h-when-a-program-reports-a-sharing-or-lock-error")

#p[#text("Error 32 is a sharing violation; error 33 is a lock violation; error 36 indicates sharing-buffer overflow. Close other DOS programs or handles using the object before retrying. Open files and directories containing them cannot be deleted or renamed freely.")]

#p[#text("Locks coordinate DOS owners inside this runtime instance. They do not provide cross-machine SMB server locking. A historical error-33 built-in loader issue was corrected; persistent failures on the current build need the program name and exact operation recorded.")]

#heading(level: 2)[#text("When a game runs badly")]#label("h-when-a-game-runs-badly")

#p[#text("Return to a known configuration and change one item at a time. Try a fixed CPU profile, the memory size the game expects, the intended video adapter and matching sound resources.")]

#p[#text("More memory or a faster profile can make older software less compatible. An unrecognized executable may be a modern Windows program rather than DOS software. Copy-protected media, unusual drivers and timing-sensitive demos need individual validation.")]

#p[#text("Use the original application's manual alongside this guide. Pi-DOS cannot guarantee every old installer, Windows system or copy-protection scheme.")]

#heading(level: 2)[#text("When the clock or network is wrong")]#label("h-when-the-clock-or-network-is-wrong")

#p[#text("A correct timezone does not establish a network connection, and a correct host connection does not enable DOS networking. Review the boot regional values and NETINFO independently.")]

#p[#text("Check wired Ethernet and the chosen guest backend. Use NETUP to retry guest startup. Use ")#inline-code("DATE /S")#text(" to resume clock synchronization after manual DOS changes. For PCAP, verify the physical interface and the network's allowance for the guest's additional MAC.")]

#heading(level: 2)[#text("Collecting diagnostic logs")]#label("h-collecting-diagnostic-logs")

#p(sticky: true)[#text("Diagnostics are off by default. Enable Diagnostic logs in setup and reboot, or use the following saved configuration setting:")]

#code-block("[pidos]\ndiagnostics=true")

#reference-table((0.85fr, 1.15fr), (
[#text("File")],
[#text("Information")],
[#inline-code("C:\\SYSTEM\\BOOT.LOG")],
[#text("Boot, storage and lifecycle")],
[#inline-code("C:\\SYSTEM\\RUNTIME.LOG")],
[#text("Runtime and backend errors")],
[#inline-code("C:\\SYSTEM\\NETWORK.LOG")],
[#text("Host DHCP, DNS and time services")],
[#inline-code("C:\\SYSTEM\\VIDEO.LOG")],
[#text("Display mode selection and presentation")],
), compact: false)

#procedure[#text("To preserve a useful problem report")]

#items[#enum(start: 1, [#text("Enable diagnostics and reboot.")], [#text("Reproduce the problem once and note the steps.")], [#text("Copy the relevant logs before another reboot replaces them.")], [#text("Include Pi model, Pi-DOS version, selected profiles, connected devices and the exact message.")], [#text("Keep the saved configuration with the report if it helps explain the settings.")])]

#p[#text("With logging disabled, old log files remain untouched and can be stale. Missing or read-only C: prevents persistent logs. Serial diagnostics remain available for developer investigation, but there is no general Linux login console in normal use.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Chapter 13 Updating and Maintaining Pi-DOS")]#label("h-chapter-13-updating-and-maintaining-pi-dos")

#p[#text("Back up data and configuration before an update. A compatible update can preserve C:, but changes to boot assets, defaults and data packs do not all follow the same path.")]

#heading(level: 2)[#text("Updating a compatible installation")]#label("h-updating-a-compatible-installation")

#p[#text("For an already compatible FAT32 volume-root installation, many runtime updates can replace BOOT's ")#inline-code("Image")#text(" while retaining PI-DOS data. A particular update may also need matching firmware, device trees, overlays or command-line files.")]

#procedure[#text("To prepare for an update")]

#items[#enum(start: 1, [#text("Record the current Pi-DOS version and important setup choices.")], [#text("Shut down and back up the complete PI-DOS partition.")], [#text("Retain the matching BOOT files or a copy of the old image.")], [#text("Follow the update's instructions for the exact matching boot assets.")], [#text("Eject the card safely, start it and check your files and settings.")])]

#p[#text("Do not mix firmware components merely because their filenames match. The recorded Pi 4 USB correction depends on a matched firmware pair. Some Pi 5 D0 boot paths need the appropriate DTB and ")#inline-code("bcm2712d0.dtbo")#text(" overlay.")]

#heading(level: 2)[#text("Preserving existing settings")]#label("h-preserving-existing-settings")

#p[#text("Startup normally seeds missing files and preserves customizations. An existing installation can therefore retain earlier CPU, sound or monitor defaults. Select a current preset or restore defaults on the intended setup page when you deliberately want to adopt current behavior.")]

#p[#text("Replacing Image does not automatically install every new benchmark, demo or data payload into an existing C:. Follow the pack's own merge instructions and preserve unrelated SYSTEM files.")]

#heading(level: 2)[#text("Moving from an older layout")]#label("h-moving-from-an-older-layout")

#p[#text("An older ext4 image needs a data backup, a newly written FAT32 image and restoration of user files. Replacing only the kernel does not convert its filesystem.")]

#p[#text("Older ")#inline-code("c/")#text(" and ")#inline-code("d/")#text(" wrapper directories are not migrated automatically. Move their contents to suitable locations in the new PI-DOS volume root. An old FAT32 data volume labelled DOSDATA must be relabelled PI-DOS for current detection, after confirming its layout and backing it up.")]

#p[#text("Do not restore an entire obsolete SYSTEM directory over a new installation without reviewing the configuration and launchers it contains. Preserve your old copy and merge intentional customizations.")]

#heading(level: 2)[#text("Adding the demo pack")]#label("h-adding-the-demo-pack")

#p[#text("The project's ")#inline-code("make demos")#text(" workflow produces ")#inline-code("output/pi-dos-demos.zip")#text(". To add that pack to an existing card, merge its DEMOS directory and SYSTEM\\DEMOS.BAT into the PI-DOS root while retaining other SYSTEM files.")]

#p[#text("This archive is produced on a development computer; it is not generated by typing make at the Pi-DOS prompt. Appendix D summarizes image-building boundaries.")]

#heading(level: 2)[#text("Maintenance habits")]#label("h-maintenance-habits")

#p[#text("Save documents and games before rebooting. Keep independent backups, especially of ZIP overlays. Wait for writes to complete before removing media. Retain the source image and matching boot assets used for a working configuration.")]

#p[#text("When a fault appears after an update, compare the version, saved configuration and matching assets before trying unrelated hardware settings. A short record of what changed is often more useful than repeated reinstallations.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Appendix A Command Quick Reference")]#label("h-appendix-a-command-quick-reference")

#p[#text("This appendix lists commands used in this guide. It is not a complete inventory of the inherited DOS engine. Syntax examples assume the standard Pi-DOS shell and actual files at the illustrated paths.")]

#heading(level: 2)[#text("Everyday DOS commands")]#label("h-everyday-dos-commands")

#reference-table((0.9fr, 1.1fr), (
[#text("Command")],
[#text("Purpose / example")],
[#inline-code("DIR")],
[#text("List the current directory")],
[#inline-code("DIR /P")],
[#text("Page a long listing")],
[#inline-code("DIR /W")],
[#text("Use a compact wide listing")],
[#inline-code("DIR /A")],
[#text("Include attribute-aware entries")],
[#inline-code("DIR /-Z")#text(" / ")#inline-code("DIR /Z")],
[#text("Long-name / short-name view")],
[#inline-code("CD \\DOCS")],
[#text("Change directory from the drive root")],
[#inline-code("CD ..")],
[#text("Go to the parent directory")],
[#inline-code("MD WORK")],
[#text("Make a directory")],
[#inline-code("RD WORK")],
[#text("Remove an empty directory")],
[#inline-code("COPY source destination")],
[#text("Copy a named file")],
[#inline-code("REN oldname newname")],
[#text("Rename a file")],
[#inline-code("DEL filename")],
[#text("Delete a file; check the target first")],
[#inline-code("TYPE filename")],
[#text("Display a text file")],
[#inline-code("EDIT filename")],
[#text("Edit a text file")],
[#inline-code("ATTRIB filename")],
[#text("Inspect DOS file attributes")],
[#inline-code("ATTRIB +R filename")],
[#text("Mark a file read-only")],
[#inline-code("VOL E:")],
[#text("Display volume information")],
[#inline-code("LABEL E:USB GAMES")],
[#text("Set a writable removable volume label")],
[#inline-code("SET BLASTER")],
[#text("Show the current sound environment")],
[#inline-code("VER")#text(" / ")#inline-code("VER /R")],
[#text("Pi-DOS version / extended runtime information")],
[#inline-code("DATE")#text(", ")#inline-code("TIME")],
[#text("Inspect or manually set the DOS clock")],
[#inline-code("DATE /S")],
[#text("Resume host clock synchronization")],
), compact: false)

#heading(level: 2)[#text("Pi-DOS setup and lifecycle")]#label("h-pi-dos-setup-and-lifecycle")

#reference-table((0.9fr, 1.1fr), (
[#text("Command")],
[#text("Purpose")],
[#inline-code("PIDOSCFG")],
[#text("Open the setup utility")],
[#inline-code("PIDOSCFG /LIST")],
[#text("List CPU profiles")],
[#inline-code("PIDOSCFG /PROFILE 486-66")],
[#text("Select the fixed 486DX2 profile; reboot afterward")],
[#inline-code("PIDOSCFG /FIRSTBOOT")],
[#text("Open welcome/pending clock review")],
[#inline-code("PIDOSCFG /HELP")],
[#text("Show setup command-line help")],
[#inline-code("PIDOSCFG /CONFIG C:\\SYSTEM\\TEST.CFG")],
[#text("Use an explicitly named configuration file")],
[#inline-code("REBOOT")],
[#text("Request clean whole-Pi reboot through the disk request path")],
[#inline-code("POWEROFF")],
[#text("Request clean shutdown")],
[#inline-code("EXIT")],
[#text("At the top-level shell, also ends the runtime for shutdown")],
), compact: false)

#p[#text("An alternate configuration file is useful for advanced inspection or preparation; normal boot still reads the authoritative PIDOS.CFG. Do not expect saving TEST.CFG to change the next startup by itself.")]

#heading(level: 2)[#text("Media and video commands")]#label("h-media-and-video-commands")

#reference-table((0.9fr, 1.1fr), (
[#text("Command")],
[#text("Purpose")],
[#inline-code("MOUNT")],
[#text("List current drive mappings")],
[#inline-code("MOUNT -U D")],
[#text("Release the removable D: mapping")],
[#inline-code("IMGMOUNT D C:\\IMAGES\\GAME.ISO -t iso")],
[#text("Mount an ISO on a free D:")],
[#inline-code("IMGSWAP D 2")],
[#text("Select image 2 from the mounted set")],
[#inline-code("ZIPMOUNT E C:\\GAMES\\GAME.ZIP")],
[#text("Mount a ZIP with persistent overlay on free E:")],
[#inline-code("ZIPMOUNT E C:\\GAMES\\GAME.ZIP -ro")],
[#text("Mount read-only, including existing overlay changes")],
[#inline-code("ZIPMOUNT E -u")],
[#text("Unmount that ZIP drive")],
[#inline-code("FORMAT A: /Q")],
[#text("Destructive quick format of recognizable floppy media")],
[#inline-code("FORMAT A: /U")],
[#text("Destructive full format at existing floppy capacity")],
[#inline-code("VIDEOMOD /P")],
[#text("Page the mode listing")],
[#inline-code("VIDEOMOD /HOST")],
[#text("List advertised host modes")],
[#inline-code("VIDEOMOD /DOS")],
[#text("List DOS BIOS/VESA modes")],
), compact: false)

#heading(level: 2)[#text("Tools and networking commands")]#label("h-tools-and-networking-commands")

#reference-table((0.9fr, 1.1fr), (
[#text("Command")],
[#text("Purpose")],
[#inline-code("DZ")],
[#text("Start Doszip Commander")],
[#inline-code("BENCH")],
[#text("Open benchmark menu; Q returns")],
[#inline-code("DEMOS")],
[#text("Open demo menu; H gives notes, Q returns")],
[#inline-code("NETINFO")],
[#text("Inspect DOS network settings/status")],
[#inline-code("NETUP")],
[#text("Retry saved DOS network startup")],
[#inline-code("IPXNET")],
[#text("Manual IPX tunnel facilities; see installed help")],
[#inline-code("WFWNET")],
[#text("Prepare the configured WfW network stack before WIN")],
), compact: false)

#p[#text("For mTCP arguments and network-driver combinations, read NETWORK.TXT and the documentation under ")#inline-code("Z:\\NETWORK\\MTCP")#text(".")]

#heading(level: 2)[#text("Commands you should not assume are present")]#label("h-commands-you-should-not-assume-are-present")

#p[#text("Pi-DOS deliberately excludes LS, TITLE, CAPMOUSE, INTRO, SHUTDOWN.COM, BUFFERS.COM, FDISK, CFGTOOL, DSXMENU, EDLIN, COMP, FILES, FCBS, LASTDRIV and FLAGSAVE. EDIT and FC remain.")]

#p[#text("Use REBOOT and POWEROFF for lifecycle control. Do not use an unrelated MS-DOS manual as proof that a particular maintenance, compression, undelete or development utility is bundled.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Appendix B Drives, Directories and Defaults")]#label("h-appendix-b-drives-directories-and-defaults")

#p[#text("Use this appendix to locate important files and compare an installation with fresh-image settings. Saved choices on an existing card may differ.")]

#heading(level: 2)[#text("Important paths")]#label("h-important-paths")

#reference-table((0.9fr, 1.1fr), (
[#text("Path")],
[#text("Contents or role")],
[#inline-code("C:\\AUTOEXEC.BAT")],
[#text("Personal startup commands")],
[#inline-code("C:\\README.TXT")],
[#text("Configuration/startup guidance")],
[#inline-code("C:\\SYSTEM\\PIDOS.CFG")],
[#text("Authoritative saved configuration")],
[#inline-code("C:\\SYSTEM\\PIDOS.BAK")],
[#text("Immediately previous setup configuration")],
[#inline-code("C:\\SYSTEM\\PIDOS.TMP")],
[#text("Save/recovery temporary file")],
[#inline-code("C:\\SYSTEM\\CONTROL.REQ")],
[#text("Supervised batch lifecycle request; managed")],
[#inline-code("C:\\SYSTEM\\CAPTURES")],
[#text("Numbered screenshots")],
[#inline-code("C:\\SYSTEM\\MIDI")],
[#text("Supplied internal MIDI assets")],
[#inline-code("C:\\SYSTEM\\NET")],
[#text("Generated DOS networking files")],
[#inline-code("C:\\SYSTEM\\BENCH")],
[#text("Benchmark menu and payload")],
[#inline-code("C:\\PROGRAMS\\TOOLS")],
[#text("Writable editor and personal tools")],
[#inline-code("C:\\PROGRAMS\\DOSZIP")],
[#text("File manager, preferences and notices")],
[#inline-code("C:\\GAMES")#text(", ")#inline-code("C:\\DEMOS")],
[#text("User games and demo pack")],
[#inline-code("C:\\DOCS")#text(", ")#inline-code("C:\\TEMP")],
[#text("Documentation and temporary files")],
[#inline-code("Z:\\NETWORK")],
[#text("Installed networking tools and manual")],
), compact: false)

#p[#text("The directory C:\\IMAGES used in examples is a suggested user-created location, not a guaranteed pre-created directory.")]

#heading(level: 2)[#text("Fresh-image defaults")]#label("h-fresh-image-defaults")

#reference-table((0.85fr, 1.15fr), (
[#text("Area")],
[#text("Current default")],
[#text("CPU")],
[#text("Pentium 120 MHz; ")#inline-code("pentium")#text(", ")#inline-code("dynamic_rec")#text(", ")#inline-code("fixed 64180")],
[#text("DOS RAM")],
[#text("16 MiB")],
[#text("Video")],
[#text("S3 Trio64")],
[#text("Voodoo")],
[#text("Software enabled, maximum memory, Glide passthrough off")],
[#text("Monitor")],
[#text("CRT")],
[#text("Retained LCD preferences")],
[#text("1024x768, 4:3, linear filtering off")],
[#text("PC sound")],
[#text("SB Pro 2, 220h, IRQ 7, DMA 1")],
[#text("PC speaker")],
[#text("Enabled")],
[#text("MIDI")],
[#text("SC-55 SoundFont profile, assets required")],
[#text("MPU-401")],
[#text("Intelligent, port 330h, automatic IRQ")],
[#text("Physical audio")],
[#text("Primary HDMI")],
[#text("Keyboard / country")],
[#text("Spanish ")#inline-code("sp")#text(" / Spain ")#inline-code("34")],
[#text("Timezone")],
[#text("Europe/Madrid")],
[#text("Joysticks")],
[#text("Two conventional two-axis devices, timed axes, autofire off")],
[#text("DOS IPX / NE2000")],
[#text("Disabled / disabled")],
[#text("Diagnostics")],
[#text("Disabled")],
), compact: false)

#p[#text("Earlier development defaults included Pentium 133, Automatic CPU and SB16. Those are historical settings, not this edition's fresh-image defaults.")]

#heading(level: 2)[#text("Startup PATH")]#label("h-startup-path")

#p(sticky: true)[#text("The standard search path is:")]

#code-block("PATH C:\\​PROGRAMS\\​TOOLS;​C:\\​SYSTEM;​Z:\\​;​Z:\\​SYSTEM;​Z:\\​DOS;​Z:\\​BIN;​Z:\\​TEXTUTIL;​Z:\\​DEBUG;​Z:\\​PATCHING;​Z:\\​4DOS")

#p[#text("Writable startup sets TEMP and TMP to C:\\TEMP. Extend the existing path in AUTOEXEC.BAT when adding your tools rather than inadvertently discarding the supplied locations.")]

#heading(level: 2)[#text("Configuration ownership")]#label("h-configuration-ownership")

#p[#text("PIDOSCFG edits supported values in PIDOS.CFG. AUTOEXEC.BAT contains personal commands. SYSTEM\\NET holds generated network files. The host creates transient runtime configuration separately; editing generated files is not the normal way to save settings.")]

#p[#text("C: and Z: mappings and their DOS labels are protected. The host FAT32 data partition must remain labelled PI-DOS even though DOS shows C: as SYSTEM.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Appendix C Glossary")]#label("h-appendix-c-glossary")

#heading(level: 2)[#text("A to I")]#label("h-a-to-i")

#reference-table((0.85fr, 1.15fr), (
[#text("Term")],
[#text("Meaning")],
[#text("8.3 name")],
[#text("A classic DOS name with up to eight base characters and three extension characters")],
[#text("ALSA")],
[#text("The native audio and MIDI interface used by the Raspberry Pi side")],
[#text("Attribute")],
[#text("A DOS file property such as read-only, hidden, system or archive")],
[#text("Batch file")],
[#text("A BAT text file containing shell commands")],
[#text("Boot image")],
[#text("A complete system image written to a storage device")],
[#text("CPU profile")],
[#text("A global choice of emulated CPU model, core and speed policy")],
[#text("Directory")],
[#text("A container for files and subdirectories")],
[#text("DOS country")],
[#text("A selection governing DOS regional conventions")],
[#text("DPMI")],
[#text("Protected-mode services used by some DOS applications and extenders")],
[#text("Dynrec")],
[#text("Dynamic translation of guest x86 instructions into native Arm code")],
[#text("EDID")],
[#text("Information describing display/adapter modes and capabilities")],
[#text("EMS / XMS")],
[#text("Expanded / extended memory interfaces for DOS software")],
[#text("Guest")],
[#text("The emulated x86 PC and its DOS environment")],
[#text("Host")],
[#text("The native Raspberry Pi system and physical device layer")],
[#text("Image mount")],
[#text("Making a disk-image file available as an emulated drive")],
[#text("Initramfs")],
[#text("The Linux root environment embedded inside Image")],
), compact: false)

#pagebreak()

#heading(level: 2)[#text("K to Z")]#label("h-k-to-z")

#reference-table((0.85fr, 1.15fr), (
[#text("Term")],
[#text("Meaning")],
[#text("KMS/DRM")],
[#text("The native display mode and presentation interface")],
[#text("MIDI")],
[#text("Musical performance messages sent to a synthesizer or module")],
[#text("MSCDEX")],
[#text("The DOS CD-ROM interface used by programs")],
[#text("Overlay")],
[#text("Persistent changes stored beside a ZIP in its PFS directory")],
[#text("Packet driver")],
[#text("A DOS networking interface used by tools such as mTCP")],
[#text("PATH")],
[#text("Directories searched for executable commands")],
[#text("PCAP")],
[#text("Direct guest Ethernet access through a wired host interface")],
[#text("Root directory")],
[#text("The top directory on a drive, written with a backslash")],
[#text("SLIRP")],
[#text("NAT-style networking for guest TCP/IP")],
[#text("SoundFont")],
[#text("Sample-based synthesis data used by the internal MIDI profiles")],
[#text("Supervisor")],
[#text("Pi-DOS's native startup and lifecycle controller")],
[#text("TSR")],
[#text("A DOS program that terminates but remains resident")],
[#text("Volume label")],
[#text("A name stored for a filesystem or presented by a drive")],
), compact: false)

#pagebreak(weak: true)

#heading(level: 1)[#text("Appendix D Technical and Distribution Notes")]#label("h-appendix-d-technical-and-distribution-notes")

#p[#text("This appendix gives context for advanced users. Normal DOS operation does not require building the image or administering the underlying Linux layer.")]

#heading(level: 2)[#text("How the system fits together")]#label("h-how-the-system-fits-together")

#p[#text("Raspberry Pi firmware starts an ARM64 Linux kernel with an embedded initramfs. A custom native supervisor mounts data, discovers devices, reads settings and starts the DOS runtime. The runtime derives from DOSBox-X and supplies the emulated x86 processor, BIOS, DOS shell and PC devices.")]

#p[#text("Native SDL2 presents fullscreen video through KMS/DRM and GLES, without X11, Wayland or a desktop. ALSA supplies real audio and MIDI transport. USB input and storage adapters connect the physical equipment to DOS-visible devices.")]

#p[#text("Linux still schedules work and drives the hardware. Pi-DOS's dedicated DOS-machine experience does not mean x86 instructions execute natively on Arm or that the system is electrically compatible with an ISA PC.")]

#heading(level: 2)[#text("Storage layout")]#label("h-storage-layout")

#reference-table((0.85fr, 1.15fr), (
[#text("Component")],
[#text("Initial layout")],
[#text("BOOT")],
[#text("256 MiB FAT32, starting at 1 MiB")],
[#text("PI-DOS")],
[#text("512 MiB FAT32, expanded where the expected layout permits")],
[#text("Native system root")],
[#text("Embedded in Image, unpacked at boot")],
[#text("Temporary runtime state")],
[#text("RAM-backed paths")],
), compact: false)

#p[#text("There is no separate writable Linux root partition. Pi-DOS attempts to remount the volatile system root read-only. BOOT and PI-DOS remain ordinary FAT partitions; the whole card is not immutable.")]

#p[#text("Expansion checks the expected signature, sector size, partition starts, label and absence of extra partitions. It does not format an unexpected device. A failed check can lead to read-only or recovery handling. Physical power interruption during filesystem metadata changes can still damage FAT32.")]

#heading(level: 2)[#text("Building on a development computer")]#label("h-building-on-a-development-computer")

#p(sticky: true)[#text("The project uses a Buildroot external tree and pinned upstream sources. Image building requires the documented development-host tools and dependencies. These commands are entered on that development computer, not at the Pi-DOS prompt:")]

#code-block("make image-config\nJOBS=4 make image")

#p[#text("The diagnostic alternative is ")#inline-code("JOBS=4 make image-diagnostic")#text(". Images are produced under ")#inline-code("output/buildroot-arm64/images/")#text(". The build does not flash a physical card or update EEPROM automatically.")]

#p[#text("User-supplied MIDI assets are staged with the project's MIDI asset importer and must match its manifest. A runtime that works offline does not imply an offline build without cached source archives and assets.")]

#p[#text("Use the project's technical reference and build instructions for dependency versions, patch maintenance, rebuilding setup and verification. The complete development history belongs there rather than in this user guide.")]

#heading(level: 2)[#text("Software and asset notices")]#label("h-software-and-asset-notices")

#p[#text("The reviewed baseline had not selected a project-wide license for original Pi-DOS code. This guide does not select one. Components and bundled payloads retain their individual licenses, notices and distribution conditions.")]

#p[#text("DOSBox-X modifications, Doszip, mTCP, SDL2, firmware and other components have their own source/notice requirements. A public image release needs a complete component and asset review. Source availability for an engine does not automatically cover commercial game data, shareware payloads, MIDI ROMs or SoundFonts.")]

#p[#text("Preserve the original notices included with tools and data. Consult the repository's LICENSES documentation before redistributing an image or payload collection. Normal use of this guide does not grant new rights to third-party software.")]

#heading(level: 2)[#text("Editorial basis")]#label("h-editorial-basis")

#p[#text("This guide was prepared from the supplied ")#inline-code("technical-reference.md")#text(", reviewed 30 October 2026, covering Pi-DOS 1.0 at baseline ")#inline-code("ff0c194")#text(". It does not claim that new hardware tests were performed during writing.")]

#p[#text("The supplied 1993 Microsoft MS-DOS 6 Concise User's Guide was used as a reference for organization, typography and procedure layout. This guide is independently written for Pi-DOS and does not imply Microsoft endorsement or that Microsoft DOS is included.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Appendix E Compatibility and Verification Notes")]#label("h-appendix-e-compatibility-and-verification-notes")

#p[#text("Implemented support, host checks and physical confirmation are different kinds of evidence. This appendix keeps the distinction visible without interrupting every everyday procedure.")]

#heading(level: 2)[#text("What the reference records")]#label("h-what-the-reference-records")

#reference-table((0.85fr, 1.15fr), (
[#text("Area")],
[#text("Recorded evidence and practical boundary")],
[#text("Board boot and input")],
[#text("User-confirmed Pi 3 and Pi 5, and Pi 4 after matching firmware correction; other targets need checks")],
[#text("Reboot and persistence")],
[#text("Appliance REBOOT, persisted test file and POWEROFF confirmed; power-loss recovery not established")],
[#text("PC execution")],
[#text("Native/Pi 5 COM, EXE, BAT and dynamic-recompiler checks; not a complete application suite")],
[#text("Display")],
[#text("Pi 5 GPU/KMS checks and user CRT appearance report; each timing/display and latest static welcome need checks")],
[#text("Keyboard/mouse")],
[#text("Virtual keyboard-path checks and user hotplug record; every application not certified")],
[#text("Controllers")],
[#text("Virtual controller checks; physical game calibration and hotplug pending")],
[#text("Audio")],
[#text("Short speaker/FM/SB checks; long audible, analogue-jack and USB acceptance pending")],
[#text("MIDI")],
[#text("Profile, identity and retry implementation; Automatic-CPU Sierra timing issue remains open")],
[#text("Floppy")],
[#text("Specific Pi 5 Toshiba/TEAC copy and format confirmation; other drives and later metadata changes need checks")],
[#text("Optical / USB data")],
[#text("Host/backend checks and ARM64 integration; physical media, audio and removal acceptance pending")],
[#text("ZIP / sharing / attributes")],
[#text("Focused DOS/backend checks; every application and physical device not certified")],
[#text("Networking")],
[#text("Configuration, host/backend and packaging checks; physical Pi LAN, native IPX and WfW sharing pending")],
[#text("Expansion")],
[#text("Native sparse-image checks at 2 and 64 GiB; ARM64 refresh and physical-card acceptance pending")],
[#text("Benchmarks / demos")],
[#text("Archive, inventory and launch-file checks; physical scores and playback synchronization pending")],
[#text("Screenshots / image cycling")],
[#text("Dispatch/configuration/image checks; physical Pi capture and in-game cycling pending")],
), compact: false)

#heading(level: 2)[#text("Reporting a new result")]#label("h-reporting-a-new-result")

#p[#text("Record the exact Pi-DOS build, Raspberry Pi model, firmware/boot assets, peripheral model, settings, media and steps. State what you observed rather than extending one success to a whole family of devices.")]

#p[#text("A successful compile or image inventory check establishes packaging, not a visible display, audible sound or correct game timing. A successful file copy on one floppy drive does not establish all formats or all later metadata paths.")]

#p[#text("Use a disposable test file or backup copy when checking writes. For normal work, prefer a configuration already known to suit your software and equipment. Keep new results with the project's technical documentation so a later guide can describe them accurately.")]

#pagebreak(weak: true)

#heading(level: 1)[#text("Index")]#label("h-index")

#p[#text("Use this alphabetical index to find a subject. Section links are clickable in the Markdown and PDF editions; the PDF also gives page references.")]

#reference-table((0.85fr, 1.15fr), (
[#text("Subject")],
[#text("Section")],
[#text("8.3 filenames")],
[#link(label("h-filenames-and-extensions"))[#text("Filenames and extensions")]#text(" (p. ")#ref(label("h-filenames-and-extensions"), form: "page", supplement: none)#text(")")],
[#text("Application compatibility")],
[#link(label("h-checking-compatibility-methodically"))[#text("Checking compatibility methodically")]#text(" (p. ")#ref(label("h-checking-compatibility-methodically"), form: "page", supplement: none)#text(")")],
[#text("Attributes")],
[#link(label("h-file-attributes-and-open-files"))[#text("File attributes and open files")]#text(" (p. ")#ref(label("h-file-attributes-and-open-files"), form: "page", supplement: none)#text(")")],
[#text("Audio output")],
[#link(label("h-choosing-the-physical-output"))[#text("Choosing the physical output")]#text(" (p. ")#ref(label("h-choosing-the-physical-output"), form: "page", supplement: none)#text(")")],
[#text("AUTOEXEC.BAT")],
[#link(label("h-customizing-user-startup"))[#text("Customizing user startup")]#text(" (p. ")#ref(label("h-customizing-user-startup"), form: "page", supplement: none)#text(")")],
[#text("Backups, configuration")],
[#link(label("h-setup-backups-and-interrupted-saves"))[#text("Setup backups and interrupted saves")]#text(" (p. ")#ref(label("h-setup-backups-and-interrupted-saves"), form: "page", supplement: none)#text(")")],
[#text("Backups, user files")],
[#link(label("h-backing-up-your-work"))[#text("Backing up your work")]#text(" (p. ")#ref(label("h-backing-up-your-work"), form: "page", supplement: none)#text(")")],
[#text("Benchmarks")],
[#link(label("h-running-benchmarks"))[#text("Running benchmarks")]#text(" (p. ")#ref(label("h-running-benchmarks"), form: "page", supplement: none)#text(")")],
[#text("Board support")],
[#link(label("h-what-you-need"))[#text("What you need")]#text(" (p. ")#ref(label("h-what-you-need"), form: "page", supplement: none)#text(")")],
[#text("Boot image preparation")],
[#link(label("h-preparing-the-system-card"))[#text("Preparing the system card")]#text(" (p. ")#ref(label("h-preparing-the-system-card"), form: "page", supplement: none)#text(")")],
[#text("C: unavailable")],
[#link(label("h-when-the-system-reaches-z-instead-of-c"))[#text("When the system reaches Z: instead of C:")]#text(" (p. ")#ref(label("h-when-the-system-reaches-z-instead-of-c"), form: "page", supplement: none)#text(")")],
[#text("CD images")],
[#link(label("h-mounting-a-cd-image"))[#text("Mounting a CD image")]#text(" (p. ")#ref(label("h-mounting-a-cd-image"), form: "page", supplement: none)#text(")")],
[#text("CDs, physical")],
[#link(label("h-using-physical-cds"))[#text("Using physical CDs")]#text(" (p. ")#ref(label("h-using-physical-cds"), form: "page", supplement: none)#text(")")],
[#text("Clock synchronization")],
[#link(label("h-reviewing-date-and-time"))[#text("Reviewing date and time")]#text(" (p. ")#ref(label("h-reviewing-date-and-time"), form: "page", supplement: none)#text(")")],
[#text("Command inventory")],
[#link(label("h-appendix-a-command-quick-reference"))[#text("Appendix A Command Quick Reference")]#text(" (p. ")#ref(label("h-appendix-a-command-quick-reference"), form: "page", supplement: none)#text(")")],
[#text("CONFIG.SYS")],
[#link(label("h-configsys-and-familiar-ms-dos-advice"))[#text("CONFIG.SYS and familiar MS-DOS advice")]#text(" (p. ")#ref(label("h-configsys-and-familiar-ms-dos-advice"), form: "page", supplement: none)#text(")")],
[#text("Controllers")],
[#link(label("h-choosing-a-joystick-or-gamepad-mode"))[#text("Choosing a joystick or gamepad mode")]#text(" (p. ")#ref(label("h-choosing-a-joystick-or-gamepad-mode"), form: "page", supplement: none)#text(")")],
[#text("Copying files")],
[#link(label("h-copying-and-renaming-a-file"))[#text("Copying and renaming a file")]#text(" (p. ")#ref(label("h-copying-and-renaming-a-file"), form: "page", supplement: none)#text(")")],
[#text("CPU profiles")],
[#link(label("h-selecting-a-cpu-profile"))[#text("Selecting a CPU profile")]#text(" (p. ")#ref(label("h-selecting-a-cpu-profile"), form: "page", supplement: none)#text(")")],
[#text("CRT displays")],
[#link(label("h-using-a-crt-monitor"))[#text("Using a CRT monitor")]#text(" (p. ")#ref(label("h-using-a-crt-monitor"), form: "page", supplement: none)#text(")")],
[#text("Date and time")],
[#link(label("h-reviewing-date-and-time"))[#text("Reviewing date and time")]#text(" (p. ")#ref(label("h-reviewing-date-and-time"), form: "page", supplement: none)#text(")")],
[#text("Deleting files")],
[#link(label("h-deleting-the-practice-files"))[#text("Deleting the practice files")]#text(" (p. ")#ref(label("h-deleting-the-practice-files"), form: "page", supplement: none)#text(")")],
[#text("Demos")],
[#link(label("h-running-demos"))[#text("Running demos")]#text(" (p. ")#ref(label("h-running-demos"), form: "page", supplement: none)#text(")")],
[#text("Diagnostic logs")],
[#link(label("h-collecting-diagnostic-logs"))[#text("Collecting diagnostic logs")]#text(" (p. ")#ref(label("h-collecting-diagnostic-logs"), form: "page", supplement: none)#text(")")],
[#text("Directories, creating")],
[#link(label("h-creating-and-changing-directories"))[#text("Creating and changing directories")]#text(" (p. ")#ref(label("h-creating-and-changing-directories"), form: "page", supplement: none)#text(")")],
[#text("Directories, system map")],
[#link(label("h-important-paths"))[#text("Important paths")]#text(" (p. ")#ref(label("h-important-paths"), form: "page", supplement: none)#text(")")],
[#text("Disc switching")],
[#link(label("h-using-more-than-one-image"))[#text("Using more than one image")]#text(" (p. ")#ref(label("h-using-more-than-one-image"), form: "page", supplement: none)#text(")")],
[#text("DOS basics")],
[#link(label("h-chapter-2-learning-dos-basics"))[#text("Chapter 2 Learning DOS Basics")]#text(" (p. ")#ref(label("h-chapter-2-learning-dos-basics"), form: "page", supplement: none)#text(")")],
[#text("Doszip Commander")],
[#link(label("h-using-doszip-commander"))[#text("Using Doszip Commander")]#text(" (p. ")#ref(label("h-using-doszip-commander"), form: "page", supplement: none)#text(")")],
[#text("Drive letters")],
[#link(label("h-understanding-drive-assignments"))[#text("Understanding drive assignments")]#text(" (p. ")#ref(label("h-understanding-drive-assignments"), form: "page", supplement: none)#text(")")],
[#text("EDIT")],
[#link(label("h-creating-a-small-text-file"))[#text("Creating a small text file")]#text(" (p. ")#ref(label("h-creating-a-small-text-file"), form: "page", supplement: none)#text(")")],
[#text("External MIDI")],
[#link(label("h-connecting-an-external-midi-module"))[#text("Connecting an external MIDI module")]#text(" (p. ")#ref(label("h-connecting-an-external-midi-module"), form: "page", supplement: none)#text(")")],
[#text("Factory defaults")],
[#link(label("h-fresh-image-defaults"))[#text("Fresh-image defaults")]#text(" (p. ")#ref(label("h-fresh-image-defaults"), form: "page", supplement: none)#text(")")],
[#text("File sharing and locks")],
[#link(label("h-when-a-program-reports-a-sharing-or-lock-error"))[#text("When a program reports a sharing or lock error")]#text(" (p. ")#ref(label("h-when-a-program-reports-a-sharing-or-lock-error"), form: "page", supplement: none)#text(")")],
[#text("First-boot setup")],
[#link(label("h-completing-the-welcome-screens"))[#text("Completing the welcome screens")]#text(" (p. ")#ref(label("h-completing-the-welcome-screens"), form: "page", supplement: none)#text(")")],
[#text("Floppy disks")],
[#link(label("h-using-a-usb-floppy-drive"))[#text("Using a USB floppy drive")]#text(" (p. ")#ref(label("h-using-a-usb-floppy-drive"), form: "page", supplement: none)#text(")")],
[#text("Formatting floppies")],
[#link(label("h-formatting-a-floppy"))[#text("Formatting a floppy")]#text(" (p. ")#ref(label("h-formatting-a-floppy"), form: "page", supplement: none)#text(")")],
[#text("Game timing")],
[#link(label("h-fixed-speed-and-automatic"))[#text("Fixed speed and Automatic")]#text(" (p. ")#ref(label("h-fixed-speed-and-automatic"), form: "page", supplement: none)#text(")")],
[#text("Image sets")],
[#link(label("h-using-more-than-one-image"))[#text("Using more than one image")]#text(" (p. ")#ref(label("h-using-more-than-one-image"), form: "page", supplement: none)#text(")")],
[#text("IPX multiplayer")],
[#link(label("h-playing-through-an-ipx-tunnel"))[#text("Playing through an IPX tunnel")]#text(" (p. ")#ref(label("h-playing-through-an-ipx-tunnel"), form: "page", supplement: none)#text(")")],
[#text("Keyboard layout")],
[#link(label("h-changing-regional-settings"))[#text("Changing regional settings")]#text(" (p. ")#ref(label("h-changing-regional-settings"), form: "page", supplement: none)#text(")")],
[#text("LCD displays")],
[#link(label("h-using-a-flat-panel-display"))[#text("Using a flat-panel display")]#text(" (p. ")#ref(label("h-using-a-flat-panel-display"), form: "page", supplement: none)#text(")")],
[#text("Licenses and assets")],
[#link(label("h-software-and-asset-notices"))[#text("Software and asset notices")]#text(" (p. ")#ref(label("h-software-and-asset-notices"), form: "page", supplement: none)#text(")")],
[#text("Long filenames")],
[#link(label("h-filenames-and-extensions"))[#text("Filenames and extensions")]#text(" (p. ")#ref(label("h-filenames-and-extensions"), form: "page", supplement: none)#text(")")],
[#text("Memory")],
[#link(label("h-choosing-dos-memory"))[#text("Choosing DOS memory")]#text(" (p. ")#ref(label("h-choosing-dos-memory"), form: "page", supplement: none)#text(")")],
[#text("MIDI assets")],
[#link(label("h-choosing-a-midi-profile"))[#text("Choosing a MIDI profile")]#text(" (p. ")#ref(label("h-choosing-a-midi-profile"), form: "page", supplement: none)#text(")")],
[#text("Migration")],
[#link(label("h-moving-from-an-older-layout"))[#text("Moving from an older layout")]#text(" (p. ")#ref(label("h-moving-from-an-older-layout"), form: "page", supplement: none)#text(")")],
[#text("Mixer")],
[#link(label("h-adjusting-the-mixer"))[#text("Adjusting the mixer")]#text(" (p. ")#ref(label("h-adjusting-the-mixer"), form: "page", supplement: none)#text(")")],
[#text("Network, direct LAN")],
[#link(label("h-connecting-directly-to-a-wired-lan"))[#text("Connecting directly to a wired LAN")]#text(" (p. ")#ref(label("h-connecting-directly-to-a-wired-lan"), form: "page", supplement: none)#text(")")],
[#text("Network, TCP/IP")],
[#link(label("h-enabling-dos-tcpip-with-slirp"))[#text("Enabling DOS TCP/IP with SLIRP")]#text(" (p. ")#ref(label("h-enabling-dos-tcpip-with-slirp"), form: "page", supplement: none)#text(")")],
[#text("PATH")],
[#link(label("h-customizing-user-startup"))[#text("Customizing user startup")]#text(" (p. ")#ref(label("h-customizing-user-startup"), form: "page", supplement: none)#text(")")],
[#text("Paths")],
[#link(label("h-understanding-paths"))[#text("Understanding paths")]#text(" (p. ")#ref(label("h-understanding-paths"), form: "page", supplement: none)#text(")")],
[#text("PIDOS.CFG")],
[#link(label("h-understanding-pidoscfg"))[#text("Understanding PIDOS.CFG")]#text(" (p. ")#ref(label("h-understanding-pidoscfg"), form: "page", supplement: none)#text(")")],
[#text("PIDOSCFG controls")],
[#link(label("h-opening-setup"))[#text("Opening setup")]#text(" (p. ")#ref(label("h-opening-setup"), form: "page", supplement: none)#text(")")],
[#text("Poweroff")],
[#link(label("h-turning-off-or-restarting"))[#text("Turning off or restarting")]#text(" (p. ")#ref(label("h-turning-off-or-restarting"), form: "page", supplement: none)#text(")")],
[#text("Read-only storage")],
[#link(label("h-when-c-is-read-only"))[#text("When C: is read-only")]#text(" (p. ")#ref(label("h-when-c-is-read-only"), form: "page", supplement: none)#text(")")],
[#text("Reboot")],
[#link(label("h-turning-off-or-restarting"))[#text("Turning off or restarting")]#text(" (p. ")#ref(label("h-turning-off-or-restarting"), form: "page", supplement: none)#text(")")],
[#text("Recovery")],
[#link(label("h-chapter-12-diagnosing-and-solving-problems"))[#text("Chapter 12 Diagnosing and Solving Problems")]#text(" (p. ")#ref(label("h-chapter-12-diagnosing-and-solving-problems"), form: "page", supplement: none)#text(")")],
[#text("Regional settings")],
[#link(label("h-changing-regional-settings"))[#text("Changing regional settings")]#text(" (p. ")#ref(label("h-changing-regional-settings"), form: "page", supplement: none)#text(")")],
[#text("Screenshots")],
[#link(label("h-taking-a-screenshot"))[#text("Taking a screenshot")]#text(" (p. ")#ref(label("h-taking-a-screenshot"), form: "page", supplement: none)#text(")")],
[#text("Shortcuts")],
[#link(label("h-system-shortcut-keys"))[#text("System shortcut keys")]#text(" (p. ")#ref(label("h-system-shortcut-keys"), form: "page", supplement: none)#text(")")],
[#text("Sound card setup")],
[#link(label("h-choosing-a-pc-sound-device"))[#text("Choosing a PC sound device")]#text(" (p. ")#ref(label("h-choosing-a-pc-sound-device"), form: "page", supplement: none)#text(")")],
[#text("Timezone")],
[#link(label("h-changing-regional-settings"))[#text("Changing regional settings")]#text(" (p. ")#ref(label("h-changing-regional-settings"), form: "page", supplement: none)#text(")")],
[#text("Updates")],
[#link(label("h-updating-a-compatible-installation"))[#text("Updating a compatible installation")]#text(" (p. ")#ref(label("h-updating-a-compatible-installation"), form: "page", supplement: none)#text(")")],
[#text("USB data storage")],
[#link(label("h-using-usb-sticks-and-external-disks"))[#text("Using USB sticks and external disks")]#text(" (p. ")#ref(label("h-using-usb-sticks-and-external-disks"), form: "page", supplement: none)#text(")")],
[#text("Validation status")],
[#link(label("h-appendix-e-compatibility-and-verification-notes"))[#text("Appendix E Compatibility and Verification Notes")]#text(" (p. ")#ref(label("h-appendix-e-compatibility-and-verification-notes"), form: "page", supplement: none)#text(")")],
[#text("Version information")],
[#link(label("h-your-first-command-prompt"))[#text("Your first command prompt")]#text(" (p. ")#ref(label("h-your-first-command-prompt"), form: "page", supplement: none)#text(")")],
[#text("Video modes")],
[#link(label("h-inspecting-available-modes"))[#text("Inspecting available modes")]#text(" (p. ")#ref(label("h-inspecting-available-modes"), form: "page", supplement: none)#text(")")],
[#text("Volume labels")],
[#link(label("h-labels-and-media-safety"))[#text("Labels and media safety")]#text(" (p. ")#ref(label("h-labels-and-media-safety"), form: "page", supplement: none)#text(")")],
[#text("Wildcards")],
[#link(label("h-using-wildcards"))[#text("Using wildcards")]#text(" (p. ")#ref(label("h-using-wildcards"), form: "page", supplement: none)#text(")")],
[#text("Windows for Workgroups")],
[#link(label("h-windows-for-workgroups"))[#text("Windows for Workgroups")]#text(" (p. ")#ref(label("h-windows-for-workgroups"), form: "page", supplement: none)#text(")")],
[#text("ZIP drives")],
[#link(label("h-mounting-a-zip-as-a-writable-drive"))[#text("Mounting a ZIP as a writable drive")]#text(" (p. ")#ref(label("h-mounting-a-zip-as-a-writable-drive"), form: "page", supplement: none)#text(")")],
[#text("ZIP saves and PFS overlays")],
[#link(label("h-keeping-zip-saves"))[#text("Keeping ZIP saves")]#text(" (p. ")#ref(label("h-keeping-zip-saves"), form: "page", supplement: none)#text(")")],
), compact: true)
