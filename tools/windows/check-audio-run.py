#!/usr/bin/env python3
"""Exercise the actual DOS audio handoff with a harmless substitute installer."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdl-library-path", type=Path, required=True,
                        help="ARM64 SDL2 library with dummy video/audio support")
    args = parser.parse_args()
    target = ROOT / "output/buildroot-arm64/target"
    qemu = ROOT / "output/tools/qemu-user/usr/bin/qemu-aarch64"
    watcom = Path(os.environ.get("WATCOM", ROOT / "output/tools/openwatcom"))
    env = dict(os.environ, WATCOM=str(watcom), INCLUDE=str(watcom / "h"),
               PATH=str(watcom / "binl64") + os.pathsep + os.environ["PATH"],
               SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy")
    for key in ("PI_DOS_SUPERVISED", "PI_DOS_SYSTEM_STORAGE", "PI_DOS_USB_FLOPPY",
                "PI_DOS_USB_CDROM", "PI_DOS_USB_STORAGE", "PI_DOS_DIAGNOSTICS", "LD_PRELOAD"):
        env.pop(key, None)
    with tempfile.TemporaryDirectory(prefix="pidos-audio-run-", dir=ROOT / "output/windows-video") as temp:
        temp = Path(temp)
        # This substitute modifies DOS startup files like the vendor installer,
        # and leaves a Windows-side marker that the handoff must retain.
        source = temp / "install.c"
        source.write_text(r'''
#include <stdio.h>
int main(void) {
    FILE *f = fopen("C:\\AUTOEXEC.BAT", "wb");
    if (!f) return 1;
    fputs("vendor autoexec\r\n", f); fclose(f);
    f = fopen("C:\\CONFIG.SYS", "wb");
    if (!f) return 1;
    fputs("vendor config\r\n", f); fclose(f);
    f = fopen("C:\\WINDOWS\\PVINST.OK", "wb");
    if (!f) return 1;
    fputs("Windows driver setup retained", f); fclose(f);
    return 0;
}
''')
        subprocess.run([str(watcom / "binl64/wcl"), "-q", "-bt=dos", "-l=dos", "-0", "-ms",
                        "-os", "-s", "-wx", "-fe=" + str(temp / "INSTALL.EXE"), str(source)],
                       cwd=temp, env=env, check=True)
        original = {"AUTOEXEC.BAT": b"@ECHO OFF\r\nREM user startup\r\n", "CONFIG.SYS": b"FILES=40\r\n"}
        for name in ("existing", "absent", "interrupted"):
            case = temp / name
            audio = case / "WINDOWS/PIDOSAUD/GUS"
            audio.mkdir(parents=True)
            shutil.copyfile(ROOT / "output/windows-video/DISK/AUDIO/GUS/PVRUN.EXE", audio / "PVRUN.EXE")
            shutil.copyfile(temp / "INSTALL.EXE", audio / "INSTALL.EXE")
            if name != "absent":
                for file, content in original.items():
                    (case / file).write_bytes(content)
            if name == "interrupted":
                (audio / "ROOTBACK.TAG").write_bytes(b"\x01\x01")
                (audio / "AUTOEXEC.PVB").write_bytes(b"preserved earlier backup")
            (case / "CHECK.BAT").write_bytes(b"@ECHO OFF\r\nC:\\WINDOWS\\PIDOSAUD\\GUS\\PVRUN.EXE >RUN.OUT\r\nEXIT\r\n")
            command = [str(qemu), "-L", str(target), "-E", "LD_LIBRARY_PATH=" + str(args.sdl_library_path.resolve()),
                       str(target / "pi-dos/pi-dos"), "-defaultconf", "-defaultmapper", "-nopromptfolder",
                       "-defaultdir", str(case), "-fastlaunch", "-nomenu", "-nogui", "-time-limit", "8"]
            for setting in ("sdl output=surface", "sdl videodriver=dummy", "sdl waitonerror=false",
                            "cpu core=normal", "cpu cycles=fixed 20000", "midi mpu401=none"):
                command += ["-set", setting]
            for dos_command in ('mount c "' + str(case) + '"', "c:", "CHECK.BAT"):
                command += ["-c", dos_command]
            with (case / "runtime.log").open("w") as log:
                result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=30)
            assert result.returncode == 0, (case / "runtime.log").read_text()
            if name == "absent":
                assert all(not (case / f).exists() for f in original), (case / "RUN.OUT").read_bytes()
            else:
                assert all((case / f).read_bytes() == content for f, content in original.items())
            if name == "interrupted":
                assert not (case / "WINDOWS/PVINST.OK").exists()
                assert (audio / "AUTOEXEC.PVB").read_bytes() == b"preserved earlier backup"
            else:
                assert (case / "WINDOWS/PVINST.OK").is_file(), (case / "RUN.OUT").read_bytes()
                assert (case / "WINDOWS/PVINST.OK").read_text() == "Windows driver setup retained", (case / "RUN.OUT").read_bytes()
                assert not (audio / "ROOTBACK.TAG").exists()
    print("PASS: actual DOS handoff preserves existing/absent startup files, keeps installer changes and refuses interrupted recovery state")


if __name__ == "__main__":
    main()
