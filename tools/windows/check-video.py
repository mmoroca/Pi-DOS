#!/usr/bin/env python3
"""Focused host checks for the Win16 package and its actual C file transaction."""
import ctypes
import importlib.util
import json
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
        assert not data.startswith(package.SZDD), f"Still compressed: {name}"
        if name.endswith((".DRV", ".DLL", ".FON")):
            assert data[:2] == b"MZ", f"Invalid expanded driver/font: {name}"
    # Win16 NE exports are uppercase; the native launcher requests INSTALL.
    assert {"INSTALL", "CPLAPPLET", "WEP"} <= exports(out / "DISK/PIVIDEO.CPL")
    exports(out / "DISK/SETUP.EXE")

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
    print("PASS: EDID/mode catalogue, expanded payloads, Win16 exports, SZDD errors and C transaction rollback")


if __name__ == "__main__":
    main()
