Pi-DOS Benchmark Pack 1.0
=========================

Type BENCH from any DOS directory. The pack lives in C:\SYSTEM\BENCH.
Exit with Q to return to your previous drive and directory.

Benchmarks
----------
1: 3DBench 1.0c (the newer version for faster PCs).
2/3: Chris's 3D Benchmark, VGA 320x200 or VESA 640x480.
4/5: PC Player Benchmark, VGA 320x200 or VESA 640x480.
     Read the FPS counter; press ESC when finished.
6/7: Doom 1.9 shareware, DEMO3, minimum or maximum detail.
     DOOMFPS calculates FPS from the result left on the text screen.
     If it cannot find the result, FPS = 74690 / realtics for DEMO3.
8/9/A: Quake 1.06 shareware, DEMO1, 320x200 / 360x480 / 640x480.
       When the timedemo ends, read the FPS in the console.
       Type QUIT and press ENTER to return to the benchmark menu.
       Verify Quake's displayed resolution: unsupported modes can fall back.
       Quake arguments are case-sensitive: use timedemo demo1 in its console.

Comparing results
-----------------
The menu leaves CPU, cycles, RAM and video hardware settings unchanged.
Use PIDOSCFG, save and reboot when deliberately changing a profile.
Default Pentium / 16 MiB / S3 Trio64 is the intended starting point.
Quake requires an enabled FPU; 640x480 tests need a VESA-capable adapter.
Sound is disabled for Doom and Quake benchmarks, matching Phil's settings.

Record the Pi model, Pi-DOS version, CPU profile/cycles, RAM, video adapter,
monitor settings, benchmark option and result. Compare the same settings.
Scores measure the configured emulated DOS environment, not bare ARM speed.
A profile's MHz label does not guarantee matching real-PC benchmark scores.
Host load, cooling and throttling can affect results if emulation falls behind.

Files and credits
-----------------
Selection and menu prepared for Pi-DOS from Phil's DOS Benchmark Pack v1.6.
Superscape 3DBench, Chris Dial's benchmark and PC Player retain their rights.
Doom and Quake: id Software. These are the original SHAREWARE game versions.
Complete original installers and game documentation are retained per game.
DOOMFPS: Thomas Perl; both original helper sources are included under DOOM.
SOURCE.TXT records download URLs and SHA-256 checksums; PHIL.TXT retains
the original compilation README. FILES.JSN is the build's file inventory.
The original executables and game data have not been patched.

Only the selected benchmarks are offered. No cache/VGA tuning tools,
CPU diagnostics, old 3DBench 1.0, Display Doctor or UniVBE are installed.
The game folders retain the original shareware support files and notices.
Hardware acceptance on a physical Pi is still required.
