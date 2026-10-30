Pi-DOS Demoscene Pack 1.0
========================

Nine DOS demoscene productions: music and graphics demonstrations.
Type DEMOS at the DOS prompt, then press 1-9 to launch a production.
H opens this guide; Q returns to DOS. Most demos use ESC to exit.
Each production keeps its own directory in C:\DEMOS.
Inside opens its original SETUP before the demo: configure sound and save.

Sound and performance
---------------------
Select Sound Blaster (or SB16 where supported) in each demo's own setup.
The Pi-DOS defaults are port 220h, IRQ 7, DMA 1, high DMA 5.
If you changed them, use your configured values; SET BLASTER shows the
current environment. SB Pro stereo requires an SB Pro emulated card;
with the default SB16, select ordinary SB or SB16 where offered.
The menu does not change CPU, video, memory or sound-card settings.
Gravis Ultrasound is optional and disabled in the default Pi-DOS setup.

Start with the default CPU profile. Older productions can depend on VGA
timing and CPU speed. If a demo glitches, try a 486 profile in PIDOSCFG;
save and REBOOT. Check its original README/NFO and try no sound to isolate
audio trouble. Panic and Second Reality need EMS for Sound Blaster audio.
Stars and Inside need extended memory; keep the default 16 MB available.
If a demo hangs, use the configured clean reboot shortcut (by default
Ctrl+Alt+Delete). Returning to the menu depends on the demo exiting normally.

These releases have been checked for archive integrity and launch files.
Playback, music synchronization and speed on physical Raspberry Pi hardware
remain unverified. This pack is a curated starting selection for Pi-DOS.

Original files
--------------
All downloaded files and notices are preserved. Each production's ORIG
folder contains its unchanged ZIP archive(s), including both Second Reality
disks. SOURCE.TXT records source URLs and SHA-256 hashes. FILES.JSN records
the installed inventory. Copyright remains with the original authors;
consult their original notices before redistributing a pack or SD image.

Installing on an existing card
-----------------------------
On another computer, copy the ZIP's DEMOS directory and SYSTEM\DEMOS.BAT
to the PI-DOS partition root. Keep the SYSTEM directory's existing contents.
Eject the card cleanly and boot Pi-DOS. Type DEMOS to open the menu.
