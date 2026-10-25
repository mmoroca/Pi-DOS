# Personalized DOS editor

`EDIT.COM` is the user's personalized copy of the PEDIT Light 4.00 editor
bundled with the pinned DOSBox-X source. Imported from
`/home/rtomas/Descargas/EDIT.COM` on 2026-10-23.

SHA-256: `31e07b70ac6db2959a975c3cb10dde488de0cbb2908e4775449a93e00c63216a`.

It has the same 97,619-byte size as the upstream embedded `EDIT.COM`; only
five bytes differ, at offsets `0x154f2`, `0x154f3`, `0x154f4`, `0x154fe` and
`0x15509`, within its settings block. Preserve the upstream component notices.

`post-image.sh` installs this seed as `C:\SYSTEM\EDIT.COM` on DOSDATA.
The existing DOS PATH selects it before the read-only editor on Z:.
Preferences saved by PEDIT modify this writable executable directly.

## ARJ

`ARJ.EXE` is the user's MS-DOS ARJ 2.41 binary, copied from
`/home/rtomas/Descargas/arj.exe`. It identifies itself as shareware,
copyright 1990-93 Robert K Jung. Its original notices are preserved.

SHA-256: `bf37548e55ea0fed2520e5f91449b2566292b2437165dc27db8e599e2d539010`.

The post-build hook installs it in `/armdos/drivez/BIN/ARJ.EXE`. DOSBox-X's
existing drivez extension exposes it as `Z:\BIN\ARJ.EXE`; the original Z: layout
and built-in utilities are retained.
