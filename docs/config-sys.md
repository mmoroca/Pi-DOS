# Writable DOS startup configuration

`C:\CONFIG.SYS` is the persistent, writable configuration file for DOS installers.
Pi-DOS seeds a minimal commented template only when the file is missing and
preserves all installer/user edits. Reboot to apply changes.

At startup, PID 1 reads it into a temporary runtime configuration. Settings
apply before DOS initializes, and C: is registered before configuration commands
run. Compatible drivers load before `AUTOEXEC.BAT`. The original file is never
rewritten during import.

`Z:\CONFIG.SYS` remains the read-only internal configuration view. Installers
should read and edit `C:\CONFIG.SYS`; making Z: writable is unnecessary.

Supported directives:

| Directive | Behavior |
| --- | --- |
| `FILES`, `FCBS`, `DOS`, `LASTDRIVE`, `BREAK`, `NUMLOCK`, `COUNTRY` | Apply through the runtime's existing DOS settings |
| `DEVICE`, `DEVICEHIGH` | Load drivers through the existing DOS device loader |
| `INSTALL`, `INSTALLHIGH` | Run startup programs through the existing DOS shell |
| `SET name=value` | Set an environment variable before AUTOEXEC |
| `REM`, `;` comments | Preserve in the source file; skip during import |

For example, if the corresponding driver is installed:

```dos
FILES=60
DEVICE=C:\WINDOWS\IFSHLP.SYS
```

Drivers still require compatibility with the internal DOS kernel. Processing a
`DEVICE` line does not make arbitrary MS-DOS drivers compatible. Windows 3.x
installation and actual Windows drivers still need physical Pi validation.

`HIMEM.SYS` and `EMM386.EXE` are skipped because XMS and EMS already come from
the runtime. `BUFFERS`, `STACKS`, `SHELL`, `IFS` and other unsupported directives
are skipped and logged. Boot menus and `INCLUDE` processing are not implemented;
directives inside menu sections are ignored rather than loading every choice.

Supported settings in this file take precedence over `[config]` defaults in
`C:\SYSTEM\PIDOS.CFG`. Keep hardware settings in PIDOS.CFG and environment/PATH
customizations in AUTOEXEC.BAT. Managed system startup sets its standard PATH,
TEMP and TMP after CONFIG processing.

Windows for Workgroups users should avoid loading `IFSHLP.SYS` twice: use either
their own CONFIG.SYS/network startup or the existing `WFWNET` helper. That helper
can be used without CONFIG.SYS changes, as described in [networking](networking.txt).
