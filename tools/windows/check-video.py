#!/usr/bin/env python3
"""Focused host checks for the Win16 package and its actual C file transaction."""
import ctypes
import importlib.util
import json
import hashlib
from pathlib import Path
import struct
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("package_video", Path(__file__).with_name("package-video.py"))
package = importlib.util.module_from_spec(spec)
spec.loader.exec_module(package)


def exports(path):
    data = path.read_bytes()
    assert data[:2] == b"MZ"
    ne = struct.unpack_from("<I", data, 60)[0]
    assert data[ne:ne + 2] == b"NE"
    assert data[ne + 54] == 2, "Executable must target Windows"
    cursor = ne + struct.unpack_from("<H", data, ne + 38)[0]
    result = set()
    while data[cursor]:
        length = data[cursor]
        cursor += 1
        result.add(data[cursor:cursor + length].decode("ascii"))
        cursor += length + 2
    return result


def main():
    out = ROOT / "output/windows-video"
    manifest = json.loads((out / "build.json").read_text())
    modes = manifest["modes"]
    assert len(modes) == 34
    expected = {(640, 480), (800, 600), (1024, 768)}
    assert {(m["width"], m["height"]) for m in modes} == expected
    assert expected <= package.edid_resolutions((ROOT / "dac-crt.bin").read_bytes())
    assert not any(m["depth"] > 16 for m in modes if m["adapter"] == "ET4")
    assert all(m["depth"] <= 8 for m in modes if m["adapter"] == "ET4" and m["width"] == 1024)
    assert len({(m["adapter"], m["width"], m["height"], m["depth"], m["dpi"]) for m in modes}) == len(modes)
    for mode in modes:
        ops = {(file, section.lower(), key.lower()): value for file, section, key, value in mode["ops"]}
        assert ops["SYSTEM.INI", "boot.description", "display.drv"] == mode["description"]
        assert ops["SYSTEM.INI", "boot.description", "aspect"] == f"100,{mode['dpi']},{mode['dpi']}"
        if mode["adapter"] == "S3":
            assert int(ops["SYSTEM.INI", "display", "screen-size"]) == mode["width"]
            assert int(ops["SYSTEM.INI", "display", "color-format"]) == mode["depth"]
        for key in ("display.drv", "286grabber", "386grabber", "fonts.fon", "fixedfon.fon", "oemfonts.fon"):
            assert (out / "DISK" / mode["adapter"] / ops["SYSTEM.INI", "boot", key]).is_file()
    for name in manifest["package_sha256"]:
        data = (out / "DISK" / name).read_bytes()
        assert hashlib.sha256(data).hexdigest() == manifest["package_sha256"][name]
        assert not data.startswith(package.SZDD), f"Still compressed: {name}"
        if name.endswith((".DRV", ".DLL", ".FON")):
            assert data[:2] == b"MZ", f"Invalid expanded driver/font: {name}"
    # Win16 NE exports are uppercase; the native launcher requests INSTALL.
    applet_exports = {"INSTALL", "INSTALLFROM", "UNINSTALL", "CPLAPPLET", "WEP", "CONTROLDIALOG", "OPTIONSDIALOG", "VALUEDIALOG", "AUDIODIALOG"}
    assert applet_exports | {"PIDOS"} <= exports(out / "DISK/PIDOS.CPL")
    assert applet_exports | {"PIDINST"} <= exports(out / "DISK/PIDINST.DLL")
    assert "SETUPDIALOG" in exports(out / "DISK/SETUP.EXE")
    assert not (out / "DISK/PIVIDEO.CPL").exists(), "Do not ship a duplicate legacy applet"
    audio = manifest["audio"]["drivers"]
    assert [driver["id"] for driver in audio] == ["SB2", "PRO2", "SB16", "GUS", "SPEAKER"]
    for driver in audio:
        original = ROOT / "output/windows-audio" / driver["archive"]
        assert hashlib.sha256(original.read_bytes()).hexdigest() == driver["sha256"]
        for file in driver["files"]:
            path = out / "DISK/AUDIO" / driver["id"] / file["name"]
            assert hashlib.sha256(path.read_bytes()).hexdigest() == file["sha256"]
    assert (out / "DISK/AUDIO/GUS/PVRUN.EXE").read_bytes().startswith(b"MZ")

    # Exercise literal, overlapping dictionary match and malformed SZDD input.
    header = package.SZDD + b"A\0"
    assert package.expand(header + struct.pack("<I", 3) + b"\x07ABC") == b"ABC"
    assert package.expand(header + struct.pack("<I", 6) + b"\x01A\xf0\xf2") == b"AAAAAA"
    for bad in (header, header + struct.pack("<I", 20) + b"\x01X"):
        try:
            package.expand(bad)
        except ValueError:
            pass
        else:
            raise AssertionError("Truncated SZDD accepted")

    with tempfile.TemporaryDirectory(prefix="pidos-video-check-") as temp:
        temp = Path(temp)
        # Exercise the actual upgrade/uninstall transaction, including failures
        # after one or both previous applets have already been moved aside.
        applet_object, applet_library = temp / "applet.o", temp / "applet.so"
        shim = temp / "rename.c"
        shim.write_text('''
#include <stdio.h>
#include <errno.h>
static unsigned calls, failures;
void test_fail_mask(unsigned mask) { calls = 0; failures = mask; }
int pv_test_rename(const char *from, const char *to) {
    ++calls;
    if (calls < 32 && (failures & (1u << calls))) { errno = EACCES; return -1; }
    return rename(from, to);
}
''')
        subprocess.run(["cc", "-c", "-fPIC", "-DPV_HOST", "-Drename=pv_test_rename",
                        "-Wall", "-Wextra", "-Werror", str(ROOT / "src/winvideo/applet.c"),
                        "-o", str(applet_object)], check=True)
        subprocess.run(["cc", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                        str(applet_object), str(shim), "-o", str(applet_library)], check=True)
        applet = ctypes.CDLL(str(applet_library))
        applet.pv_applet_commit.argtypes = [ctypes.c_char_p, ctypes.c_int]
        applet.pv_applet_commit.restype = ctypes.c_int
        applet.test_fail_mask.argtypes = [ctypes.c_uint]
        applet.test_fail_mask.restype = None
        apps = temp / "APPS"
        apps.mkdir()
        (apps / "SYSTEM.INI").write_bytes(b"keep user configuration")
        (apps / "VENDOR.DRV").write_bytes(b"keep installed vendor driver")

        def prepare_applets(previous):
            for name in ("PIDOS.CPL", "PIVIDEO.CPL", "PIDOS.NEW", "PIDOS.OLD", "PIVIDEO.OLD"):
                (apps / name).unlink(missing_ok=True)
            for name in previous:
                (apps / name).write_bytes(b"old:" + name.encode())
            (apps / "PIDOS.NEW").write_bytes(b"new applet")
            applet.test_fail_mask(0)

        for previous in ((), ("PIVIDEO.CPL",), ("PIDOS.CPL",), ("PIDOS.CPL", "PIVIDEO.CPL")):
            prepare_applets(previous)
            assert applet.pv_applet_commit(bytes(apps), 0) == 1
            assert (apps / "PIDOS.CPL").read_bytes() == b"new applet"
            assert not (apps / "PIVIDEO.CPL").exists()
            assert applet.pv_applet_commit(bytes(apps), 1) == 1
            assert not (apps / "PIDOS.CPL").exists()
            assert not (apps / "PIVIDEO.CPL").exists()
        for failed_call in (2, 3):
            prepare_applets(("PIDOS.CPL", "PIVIDEO.CPL"))
            applet.test_fail_mask(1 << failed_call)
            assert applet.pv_applet_commit(bytes(apps), 0) == 0
            for name in ("PIDOS.CPL", "PIVIDEO.CPL"):
                assert (apps / name).read_bytes() == b"old:" + name.encode()
            assert (apps / "PIDOS.NEW").read_bytes() == b"new applet"
        prepare_applets(("PIDOS.CPL", "PIVIDEO.CPL"))
        applet.test_fail_mask(1 << 2)
        assert applet.pv_applet_commit(bytes(apps), 1) == 0
        assert (apps / "PIDOS.CPL").read_bytes() == b"old:PIDOS.CPL"
        assert (apps / "PIVIDEO.CPL").read_bytes() == b"old:PIVIDEO.CPL"
        prepare_applets(("PIDOS.CPL", "PIVIDEO.CPL"))
        applet.test_fail_mask((1 << 2) | (1 << 3))
        assert applet.pv_applet_commit(bytes(apps), 0) == -1
        assert (apps / "PIDOS.OLD").read_bytes() == b"old:PIDOS.CPL"
        applet.test_fail_mask(0)
        assert applet.pv_applet_commit(bytes(apps), 0) == 0, "Keep interrupted recovery files"
        assert (apps / "SYSTEM.INI").read_bytes() == b"keep user configuration"
        assert (apps / "VENDOR.DRV").read_bytes() == b"keep installed vendor driver"
        audio_library = temp / "audio_ini.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "src/winvideo/audio_ini.c"), "-o", str(audio_library)], check=True)
        rewrite = ctypes.CDLL(str(audio_library)).pa_sound_vxd
        rewrite.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_char_p]
        rewrite.restype = ctypes.c_int
        original, prepared = temp / "audio-input.ini", temp / "audio-output.ini"
        devices = (b"[boot]\r\ndisplay.drv=S3.DRV\r\n[386Enh]\r\n"
                   b"device=*vpicd\r\n  device = C:\\OLD\\VSBPD.386 ; previous audio\r\n"
                   b"device=NETWORK.386\r\ndevice=vsbd.386\r\ndevice=vadlibd.386\r\n"
                   b"device=*vtd\r\n[network]\r\nkeep=yes\r\n")
        original.write_bytes(devices)
        vxd = b"C:\\WINDOWS\\PIDOSAUD\\PRO2\\VSBPD.386"
        assert rewrite(bytes(original), bytes(prepared), vxd) == 1
        output = prepared.read_bytes()
        assert output.count(b"device=" + vxd) == 1
        for keep in (b"device=*vpicd", b"device=NETWORK.386", b"device=*vtd", b"keep=yes", b"display.drv=S3.DRV"):
            assert keep in output
        assert b"C:\\OLD" not in output and b"device=vsbd.386" not in output and b"device=vadlibd.386" not in output
        original.write_bytes(output)
        assert rewrite(bytes(original), bytes(prepared), vxd) == 1
        assert prepared.read_bytes() == output, "Repeated installation must not duplicate VxDs"
        assert rewrite(bytes(original), bytes(prepared), b"") == 1
        assert b"VSBPD.386" not in prepared.read_bytes()
        original.write_bytes(b"[boot]\r\ndisplay.drv=S3.DRV")
        assert rewrite(bytes(original), bytes(prepared), vxd) == 1
        assert b"[386Enh]\r\n" in prepared.read_bytes()
        original.write_bytes(b"[386Enh]\r\n" + b"x" * 1200 + b"\r\n")
        assert rewrite(bytes(original), bytes(prepared), vxd) == 0
        assert not prepared.exists(), "Do not truncate long INI lines silently"
        library = temp / "transaction.so"
        subprocess.run(["cc", "-shared", "-fPIC", "-DPV_HOST", "-Wall", "-Wextra", "-Werror",
                        str(ROOT / "src/winvideo/transaction.c"), "-o", str(library)], check=True)
        commit = ctypes.CDLL(str(library)).pv_commit
        commit.argtypes = [ctypes.c_char_p, ctypes.c_char_p]
        commit.restype = ctypes.c_int
        current, stage = temp / "WINDOWS", temp / "STAGE"
        current.mkdir(); stage.mkdir()
        names = ("SYSTEM.INI", "WIN.INI", "PROGMAN.INI")

        def prepare():
            for name in names:
                (current / name).write_text("old:" + name)
                (stage / name).write_text("new:" + name)

        def run():
            return commit(str(current).encode(), str(stage).encode())

        prepare()
        assert run() == 1
        assert all((current / n).read_text() == "new:" + n for n in names)
        prepare()
        (stage / "WIN.INI").unlink()
        assert run() == 0
        assert all((current / n).read_text() == "old:" + n for n in names)
        prepare()
        # Force the second rename to fail AFTER the first file was committed.
        obstacle = stage / "PV1.OLD"
        obstacle.mkdir()
        (obstacle / "occupied").write_text("force rename failure")
        assert run() == 0
        assert all((current / n).read_text() == "old:" + n for n in names)
    print("PASS: package hashes, distinct Win16 applet/installer modules, legacy upgrade/reinstall/uninstall and rollback, EDID modes, sound VxDs and INI transaction")


if __name__ == "__main__":
    main()
