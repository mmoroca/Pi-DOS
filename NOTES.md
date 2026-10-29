- [ ] defrag fails on both c and d with "Divide error"
- [ ] tool to measure disk/disc w/r speed
- [ ] Add system option for audio output:
    - hdmi - default
    - jack (pi3/4)
    - external usb dac
- [ ] first boot should ask for some defaults like time/zone,. country etc
- [ ] custom formatting/partition/volume letter assigment tool

---------------------------------------------

## Pi-DOS — Complete Feature List

### DOS Environment

- [v] MS-DOS-compatible command prompt
- [v] DOS commands (DIR, COPY, DEL, etc.)
- [v] COM and EXE execution
- [v] BAT scripting
- [~] AUTOEXEC.BAT support
- [~] CONFIG.SYS compatibility
- [v] Environment variables
- [v] Standard folder structure for the DOS file system
- [v] DOS file managers (DosZip Commander)
- [v] DOS text editors
- [x] DOS programming tools (BASIC, Pascal, C, Assembly)
- [v] DOS keyboard shortcuts
- [~] DOS error messages and exit codes
- [v] International keyboard layouts and code pages
- [v] DOS date and time management
- [v] Configurable DOS version reporting

### CPU and Memory

- [v] x86 emulation (8086 through Pentium)
- [v] ARM64 dynamic recompiler
- [v] Real-mode execution
- [v] Protected-mode execution
- [v] x87 FPU emulation
- [v] Conventional memory (640 KB)
- [v] Upper memory (UMB)
- [v] Extended memory (XMS)
- [v] Expanded memory (EMS)
- [v] High Memory Area (HMA)
- [v] DPMI support
- [v] DOS extenders (DOS/4GW, etc.)
- [v] Configurable CPU speed and cycles
- [v] Automatic CPU speed adjustment
- [v] Self-modifying code support
- [~] TSR compatibility

### Graphics and Video

- [v] MDA, Hercules, CGA, EGA and VGA emulation
- [v] SVGA / VESA VBE modes
- [v] DOS text modes (80×25, etc.)
- [v] Custom text fonts
- [v] VGA Mode 13h (320×200)
- [v] High-resolution DOS graphics
- [v] Accurate VGA palettes
- [v] Hardware palette cycling
- [v] Correct 4:3 aspect ratios
- [x] Integer scaling
- [~] Pixel-perfect rendering
- [x] CRT shaders
- [x] Scanline effects
- [x] Phosphor and shadow-mask simulation
- [v] VGA 70 Hz output
- [v] CGA/EGA video timing
- [v] Direct DRM/KMS output
- [~] OpenGL ES acceleration
- [v] VSync
- [v] Low-latency video output
- [v] Fullscreen mode
- [v] HDMI resolution switching
- [v] Real CRT-compatible video modes
- [v] 3dfx Voodoo emulation

### Audio and Music

- [v] PC speaker emulation
- [v] AdLib / OPL2
- [v] OPL3
- [v] Sound Blaster
- [v] Sound Blaster Pro
- [v] Sound Blaster 16
- [v] Gravis UltraSound
- [v] Roland MT-32 emulation
- [v] General MIDI
- [v] Roland Sound Canvas / SC-55 emulation
- [v] SoundFont support
- [v] External MIDI devices
- [v] MIDI device selection
- [x] Audio latency configuration
- [v] Accurate audio timing
- [v] Direct ALSA output
- [v] Audio/video synchronization

### Storage and Filesystems

- [v] Persistent C: drive
- [v] Multiple DOS drives (C:, D:, etc.)
- [v] DOS drive-letter mapping
- [~] FAT12 / FAT16 / FAT32 disk images
- [v] Floppy disk images
- [v] Read-only disk images
- [v] Hard disk images
- [v] CD-ROM emulation
- [v] ISO image mounting
- [v] MSCDEX compatibility
- [v] Multi-disc switching
- [v] ZIP archive mounting
- [v] Host filesystem mapping
- [v] Case-insensitive filenames
- [v] 8.3 filename compatibility
- [v] DOS file attributes
- [v] DOS timestamps
- [v] DOS volume labels
- [v] USB storage mounting
- [v] Automatic USB drive mapping
- [x] Writable disk overlays
- [v] File sharing and locking
- [v] Disk caching
- [v] Persistent user configuration

### Input and Controllers

- [v] USB keyboard
- [v] USB mouse
- [v] DOS keyboard scan codes
- [v] Mouse driver emulation
- [v] International keyboard layouts
- [v] Joystick emulation
- [v] USB gamepads
- [x] Gamepad-to-keyboard mapping
- [x] Per-game controller profiles
- [x] Hotplug support
- [~] Configurable keyboard shortcuts

### DOS and PC Hardware Compatibility

- [ ] DOS INT 21h services
- [ ] BIOS INT 10h video services
- [ ] BIOS INT 13h disk services
- [ ] BIOS INT 16h keyboard services
- [ ] BIOS timer services
- [ ] Programmable Interrupt Controller (PIC)
- [ ] Programmable Interval Timer (PIT)
- [ ] DMA emulation
- [ ] VGA register emulation
- [ ] Keyboard controller emulation
- [ ] Hardware interrupt handling
- [ ] Direct I/O port emulation
- [ ] PC hardware timing compatibility
- [ ] DOS device drivers (where compatible)
- [ ] Bootable FreeDOS/MS-DOS disk images
- [ ] Alternative DOS shells such as 4DOS
- [ ] Windows 3.x compatibility
- [ ] DOS networking and IPX
- [ ] Serial port emulation
- [ ] Modem emulation
- [ ] BBS connectivity
- [ ] Multiplayer DOS gaming
- [ ] Demoscene compatibility

### Gaming and Convenience

- [ ] Per-game configuration profiles
- [ ] Automatic game configuration
- [ ] Game-specific CPU settings
- [ ] Automatic sound-card configuration
- [ ] Save states
- [ ] Load states
- [ ] Rewind
- [ ] Screenshots
- [ ] Video recording
- [ ] Gamepad-friendly navigation
- [ ] Automatic mounting of game directories
- [ ] ZIP game launching
- [ ] Automatic CD image selection
- [ ] Multi-disc management
- [ ] Optional graphical game launcher
- [ ] Optional DOS-style game browser
- [ ] Game compatibility database
- [ ] Game-specific performance optimizations

### Operating System and Boot

- [ ] Boot directly to `C:\>`
- [ ] No Linux desktop
- [ ] No X11 or Wayland
- [ ] No systemd
- [ ] No login or user accounts
- [ ] Root-only execution
- [ ] Minimal Buildroot system
- [ ] Custom PID 1
- [ ] Embedded initramfs
- [ ] Read-only system image
- [ ] Separate persistent DOS partition
- [ ] Native SDL3
- [ ] Direct DRM/KMS
- [ ] Direct ALSA
- [ ] Direct USB and input access
- [ ] Fast boot optimization
- [ ] Minimal RAM consumption
- [ ] Minimal background processes
- [ ] Silent Linux initialization
- [ ] DOS-style startup screen
- [ ] DOS-style shutdown and reboot
- [ ] Automatic hardware detection
- [ ] Raspberry Pi 3/4/5 ARM64 support
- [ ] Reproducible SD card images
- [ ] Automatic DOS storage mounting
- [ ] Recovery from filesystem errors
- [ ] Optional serial debugging
- [ ] Offline operation without networking services

Note: This is a consolidated feature wishlist, not a list of features already implemented or guaranteed compatible. It includes existing DOSBox-X capabilities, proposed Pi-DOS functionality and longer-term enhancements.
