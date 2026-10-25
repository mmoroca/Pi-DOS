- [ ] defrag fails on both c and d with "Divide error"
- [ ] command to display available video modes
- [ ] dos utility to select hdmi, jack or external dac audio output. Also enable the dosbox mixer controls.
- [ ] enable midi output
- [ ] info table on boot display some wrong character on the table double lines when chnaging the dosbox keyb lang to es

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
- [ ] DOS programming tools (BASIC, Pascal, C, Assembly)
- [v] DOS keyboard shortcuts
- [ ] DOS error messages and exit codes
- [ ] International keyboard layouts and code pages
- [ ] DOS date and time management
- [v] Configurable DOS version reporting

### CPU and Memory

- [ ] x86 emulation (8086 through Pentium)
- [ ] ARM64 dynamic recompiler
- [ ] Real-mode execution
- [ ] Protected-mode execution
- [ ] x87 FPU emulation
- [ ] Conventional memory (640 KB)
- [ ] Upper memory (UMB)
- [ ] Extended memory (XMS)
- [ ] Expanded memory (EMS)
- [ ] High Memory Area (HMA)
- [ ] DPMI support
- [ ] DOS extenders (DOS/4GW, etc.)
- [ ] Configurable CPU speed and cycles
- [ ] Automatic CPU speed adjustment
- [ ] Self-modifying code support
- [ ] TSR compatibility

### Graphics and Video

- [ ] MDA, Hercules, CGA, EGA and VGA emulation
- [ ] SVGA / VESA VBE modes
- [ ] DOS text modes (80×25, etc.)
- [ ] Custom text fonts
- [ ] VGA Mode 13h (320×200)
- [ ] High-resolution DOS graphics
- [ ] Accurate VGA palettes
- [ ] Hardware palette cycling
- [ ] Correct 4:3 aspect ratios
- [ ] Integer scaling
- [ ] Pixel-perfect rendering
- [ ] CRT shaders
- [ ] Scanline effects
- [ ] Phosphor and shadow-mask simulation
- [ ] VGA 70 Hz output
- [ ] CGA/EGA video timing
- [ ] Direct DRM/KMS output
- [ ] OpenGL ES acceleration
- [ ] VSync
- [ ] Low-latency video output
- [ ] Fullscreen mode
- [ ] HDMI resolution switching
- [ ] Real CRT-compatible video modes
- [ ] 3dfx Voodoo emulation

### Audio and Music

- [ ] PC speaker emulation
- [ ] AdLib / OPL2
- [ ] OPL3
- [ ] Sound Blaster
- [ ] Sound Blaster Pro
- [ ] Sound Blaster 16
- [ ] Gravis UltraSound
- [ ] Roland MT-32 emulation
- [ ] General MIDI
- [ ] Roland Sound Canvas / SC-55 emulation
- [ ] SoundFont support
- [ ] External MIDI devices
- [ ] MIDI device selection
- [ ] Audio latency configuration
- [ ] Accurate audio timing
- [ ] Direct ALSA output
- [ ] Audio/video synchronization

### Storage and Filesystems

- [ ] Persistent C: drive
- [ ] Multiple DOS drives (C:, D:, etc.)
- [ ] DOS drive-letter mapping
- [ ] FAT12 / FAT16 / FAT32 disk images
- [ ] Host filesystem mapping
- [ ] Case-insensitive filenames
- [ ] 8.3 filename compatibility
- [ ] DOS file attributes
- [ ] DOS timestamps
- [ ] DOS volume labels
- [ ] Floppy disk images
- [ ] Hard disk images
- [ ] CD-ROM emulation
- [ ] ISO image mounting
- [ ] MSCDEX compatibility
- [ ] Multi-disc switching
- [ ] USB storage mounting
- [ ] Automatic USB drive mapping
- [ ] ZIP archive mounting
- [ ] Read-only disk images
- [ ] Writable disk overlays
- [ ] File sharing and locking
- [ ] Disk caching
- [ ] Persistent user configuration

### Input and Controllers

- [ ] USB keyboard
- [ ] USB mouse
- [ ] DOS keyboard scan codes
- [ ] Mouse driver emulation
- [ ] International keyboard layouts
- [ ] Joystick emulation
- [ ] USB gamepads
- [ ] Gamepad-to-keyboard mapping
- [ ] Per-game controller profiles
- [ ] Hotplug support
- [ ] Configurable keyboard shortcuts

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
