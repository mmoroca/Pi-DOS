@ECHO OFF
:menu
CLS
ECHO Pi-DOS Benchmark Pack 1.0
ECHO.
ECHO 1  3DBench 1.0c
ECHO 2  Chris's 3D Benchmark       320x200
ECHO 3  Chris's 3D Benchmark       640x480
ECHO 4  PC Player Benchmark        320x200
ECHO 5  PC Player Benchmark        640x480
ECHO 6  Doom timedemo              Minimum detail
ECHO 7  Doom timedemo              Maximum detail
ECHO 8  Quake timedemo             320x200
ECHO 9  Quake timedemo             360x480
ECHO A  Quake timedemo             640x480
ECHO.
ECHO H  Instructions              Q  Return to DOS
ECHO.
ECHO Uses current CPU/video settings. Sound is disabled in game tests.
ECHO PC Player: ESC to finish. Quake: type QUIT after reading the FPS.
ECHO.
CHOICE.EXE /N /C:123456789AHQ Select benchmark:
IF ERRORLEVEL 12 GOTO end
IF ERRORLEVEL 11 GOTO help
IF ERRORLEVEL 10 GOTO quake640
IF ERRORLEVEL 9 GOTO quake360
IF ERRORLEVEL 8 GOTO quake320
IF ERRORLEVEL 7 GOTO doommax
IF ERRORLEVEL 6 GOTO doommin
IF ERRORLEVEL 5 GOTO pcp640
IF ERRORLEVEL 4 GOTO pcp320
IF ERRORLEVEL 3 GOTO chris640
IF ERRORLEVEL 2 GOTO chris320
IF ERRORLEVEL 1 GOTO bench3d
GOTO menu
:bench3d
CD 3DBENCH
3DBENCH2.EXE
GOTO finished
:chris320
CD CHRIS
BENCH.EXE
GOTO finished
:chris640
CD CHRIS
SVGABNCH.EXE
GOTO finished
:pcp320
CD PCPBENCH
PCPBENCH.EXE /VGAMODE
GOTO finished
:pcp640
CD PCPBENCH
PCPBENCH.EXE 101
GOTO finished
:doommin
CD DOOM
DOOM.EXE -CONFIG MIN.CFG -TIMEDEMO DEMO3
DOOMFPS1.EXE
GOTO finished
:doommax
CD DOOM
DOOM.EXE -CONFIG MAX.CFG -TIMEDEMO DEMO3
DOOMFPS1.EXE
GOTO finished
:quake320
CD QUAKE
QUAKE.EXE +exec mode0.cfg +timedemo demo1 -nosound -nocdaudio
GOTO finished
:quake360
CD QUAKE
QUAKE.EXE +exec mode10.cfg +timedemo demo1 -nosound -nocdaudio
GOTO finished
:quake640
CD QUAKE
QUAKE.EXE +exec mode12.cfg +timedemo demo1 -nosound -nocdaudio
GOTO finished
:finished
ECHO.
PAUSE
CD ..
GOTO menu
:help
MORE < README.TXT
PAUSE
GOTO menu
:end
