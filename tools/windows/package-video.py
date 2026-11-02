#!/usr/bin/env python3
"""Prepare user-supplied Win3.1 S3/ET4000 drivers for the Pi-DOS installer."""

import argparse
import csv
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct

ROOT = Path(__file__).resolve().parents[2]
SZDD = bytes.fromhex("535a444488f02733")


def expand(data):
    """Microsoft SZDD mode A (also used by ET4 files without underscore names)."""
    if not data.startswith(SZDD):
        return data
    if len(data) < 14 or data[8] != ord("A"):
        raise ValueError("Unsupported SZDD header")
    size = struct.unpack_from("<I", data, 10)[0]
    if size > 4 * 1024 * 1024:
        raise ValueError("Unexpectedly large driver file")
    ring, pos, cursor, result = bytearray(b" " * 4096), 4080, 14, bytearray()
    try:
        while len(result) < size:
            flags = data[cursor]
            cursor += 1
            for bit in range(8):
                if len(result) == size:
                    break
                if flags & (1 << bit):
                    values = [data[cursor]]
                    cursor += 1
                else:
                    low, high = data[cursor], data[cursor + 1]
                    cursor += 2
                    offset, count = low | ((high & 240) << 4), (high & 15) + 3
                    if len(result) + count > size:
                        raise ValueError("SZDD match exceeds declared size")
                    # Copy one byte at a time: matches can overlap the write head.
                    for index in range(count):
                        value = ring[(offset + index) & 4095]
                        result.append(value)
                        ring[pos] = value
                        pos = (pos + 1) & 4095
                    continue
                for value in values:
                    result.append(value)
                    ring[pos] = value
                    pos = (pos + 1) & 4095
    except IndexError as error:
        raise ValueError("Truncated SZDD stream") from error
    return bytes(result)


def edid_resolutions(data):
    if (len(data) < 128 or len(data) != 128 * (data[126] + 1)
            or data[:8] != bytes.fromhex("00ffffffffffff00")
            or any(sum(data[i:i + 128]) % 256 for i in range(0, len(data), 128))):
        raise ValueError("Invalid EDID header, extension count or checksum")
    modes = set()
    established = (
        (35, 7, 720, 400), (35, 6, 720, 400),
        (35, 5, 640, 480), (35, 4, 640, 480), (35, 3, 640, 480), (35, 2, 640, 480),
        (35, 1, 800, 600), (35, 0, 800, 600),
        (36, 7, 800, 600), (36, 6, 800, 600), (36, 5, 832, 624),
        (36, 4, 1024, 768), (36, 3, 1024, 768), (36, 2, 1024, 768),
        (36, 1, 1024, 768), (36, 0, 1280, 1024), (37, 7, 1152, 870),
    )
    for byte, bit, width, height in established:
        if data[byte] & (1 << bit):
            modes.add((width, height))
    for offset in range(38, 54, 2):
        a, b = data[offset:offset + 2]
        if (a, b) == (1, 1):
            continue
        width = (a + 31) * 8
        ratios = ((16, 10), (4, 3), (5, 4), (16, 9))
        x, y = ratios[b >> 6]
        modes.add((width, width * y // x))

    def dtds(start, end):
        for offset in range(start, end - 17, 18):
            d = data[offset:offset + 18]
            if d[0] or d[1]:
                modes.add((d[2] | ((d[4] & 240) << 4), d[5] | ((d[7] & 240) << 4)))

    dtds(54, 126)
    # CTA video formats present in Pi-DOS's DAC EDID. Fail closed on additions
    # instead of silently allowing a mode based on an incomplete decoder.
    vic_modes = {3: (720, 480), 4: (1280, 720), 16: (1920, 1080)}
    for start in range(128, len(data), 128):
        if data[start] != 2:
            raise ValueError("Unsupported EDID extension")
        end = data[start + 2]
        if not end:
            continue
        if not 4 <= end <= 127:
            raise ValueError("Invalid CTA data block offset")
        cursor = start + 4
        while cursor < start + end:
            tag, size = data[cursor] >> 5, data[cursor] & 31
            if cursor + size + 1 > start + end:
                raise ValueError("Truncated CTA data block")
            if tag == 2:
                for value in data[cursor + 1:cursor + size + 1]:
                    vic = value & 127
                    if vic not in vic_modes:
                        raise ValueError(f"Add EDID CTA VIC {vic} to the video pack decoder")
                    modes.add(vic_modes[vic])
            cursor += size + 1
        dtds(start + end, start + 127)
    return modes


def read_inf(path):
    sections, section = {}, None
    for line in path.read_text(encoding="cp437").splitlines():
        line = line.strip()
        if not line or line.startswith(";") or line == "\x1a":
            continue
        if line.startswith("["):
            section = line.strip("[]").lower()
            sections[section] = []
        elif section:
            sections[section].append(line)
    return sections


def fields(line):
    return [value.strip() for value in next(csv.reader([line], skipinitialspace=True))]


def filename(value):
    name = value.split(":", 1)[-1].upper()
    if not re.fullmatch(r"[A-Z0-9_]{1,8}\.[A-Z0-9_]{1,3}", name):
        raise ValueError(f"Not a DOS filename: {value}")
    return name


def make_catalog(source, output, allowed):
    modes, files, provenance = [], {}, {}
    for adapter in ("S3", "ET4"):
        folder = source / adapter
        inf = read_inf(folder / "OEMSETUP.INF")
        inputs = {p.name.upper(): p for p in folder.iterdir() if p.is_file()}

        def payload(name, target="SYSTEM"):
            name = filename(name)
            path = inputs.get(name) or inputs.get(name[:-1] + "_")
            if path is None:
                raise ValueError(f"Missing {adapter}/{name}")
            data = expand(path.read_bytes())
            dest = output / adapter / name
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(data)
            files[(adapter, name, target)] = len(data)
            provenance[f"{adapter}/{path.name}"] = hashlib.sha256(path.read_bytes()).hexdigest()
            return name

        for line in inf["display"]:
            key, row = line.split("=", 1)
            f = fields(row)
            description = f[1]
            size = re.search(r"(\d+)x(\d+)", description)
            if not size or tuple(map(int, size.groups())) not in allowed:
                continue
            driver = filename(f[0])
            # Use the current flat S3 driver, not its special legacy 1 MB path.
            if adapter == "S3" and driver != "S3TRIO.DRV":
                continue
            colour = re.search(r"\d+x\d+\s+(16\.7(?:M| million colors)|65535|32768|64K|32K|256|16)\b", description)
            if not colour:
                raise ValueError(f"Unknown colour format: {description}")
            depth = {"16": 4, "256": 8, "32K": 15, "32768": 15,
                     "64K": 16, "65535": 16, "16.7M": 32,
                     "16.7 million colors": 24}[colour[1]]
            # ET4000 emulation implements Sierra 15/16-bit DAC modes, not 24-bit.
            if adapter == "ET4" and depth == 24:
                continue
            dpi = int(f[2].split(",")[1])
            # Ordered INI operations: filename, section, key, value (None deletes).
            ops = [
                ("SYSTEM.INI", "boot", "display.drv", payload(f[0])),
                ("SYSTEM.INI", "boot", "286grabber", payload(f[3])),
                ("SYSTEM.INI", "boot", "386grabber", payload(f[6])),
                ("SYSTEM.INI", "386Enh", "display", payload(f[5])),
                ("SYSTEM.INI", "boot.description", "display.drv", description),
                ("SYSTEM.INI", "boot.description", "aspect", f[2]),
            ]
            # Preserve exact OEM descriptions: ET4 scans these for its mode size.
            for section, boot_key in (("sysfonts", "fonts.fon"), ("fixedfonts", "fixedfon.fon"),
                                      ("oemfonts", "oemfonts.fon")):
                font = next(fields(x)[0] for x in inf[section] if fields(x)[2] == f[2])
                ops.append(("SYSTEM.INI", "boot", boot_key, payload(font)))
            for entry in inf.get(filename(f[6]).lower(), []):
                ff = fields(entry)
                name = payload(ff[0])
                for value in ff[1:]:
                    payload(value)
                ops.append(("SYSTEM.INI", "386Enh", name, name))
            for entry in inf["fonts"]:
                ff = fields(entry)
                if ff[2] in (f[2], "CONTINUOUSSCALING"):
                    ops.append(("WIN.INI", "fonts", ff[1], payload(ff[0])))
            if len(f) > 9:
                for entry in inf[f[9].lower()]:
                    ff = fields(entry)
                    if ff[0]:
                        target = "SYSTEM" if ff[1].lower() == "0:system" else "WINDOWS"
                        payload(ff[0], target)
                    if len(ff) >= 6:
                        value = ff[5]
                        name = ff[4].rstrip("=")
                        if value:
                            name, value = value.split("=", 1)
                        ops.append((ff[2].upper(), ff[3], name, value.strip() if value else None))
            # Remove S3 DCI hooks when selecting ET4000, whose package has none.
            if adapter == "ET4":
                ops.extend([("SYSTEM.INI", "drivers", "DCI", None),
                            ("SYSTEM.INI", "drivers", "VIDS.DRAW", None)])
            modes.append(dict(adapter=adapter, key=key.strip(), description=description,
                              width=int(size[1]), height=int(size[2]), depth=depth, dpi=dpi, ops=ops))
        # Keep the original vendor instructions/INF available for manual setup.
        for name in ("OEMSETUP.INF", "README.DOC", "VERSION"):
            if name in inputs:
                dest = output / adapter / name
                dest.parent.mkdir(parents=True, exist_ok=True)
                dest.write_bytes(inputs[name].read_bytes())

    # Runtime and package share the same immutable catalog; no arbitrary modes.
    def cstr(value):
        return "NULL" if value is None else json.dumps(value, ensure_ascii=True)

    code = ["/* Generated from supplied OEMSETUP.INF files and dac-crt.bin. */"]
    for index, mode in enumerate(modes):
        code.append(f"static const PvOp ops{index}[] = {{")
        for file, section, key, value in mode["ops"]:
            if file not in ("SYSTEM.INI", "WIN.INI", "PROGMAN.INI"):
                raise ValueError(f"Unexpected INI target: {file}")
            code.append("    {" + ",".join(cstr(x) for x in (file, section, key, value)) + "},")
        code.append("};")
    code.append("static const PvMode modes[] = {")
    for index, mode in enumerate(modes):
        values = [cstr(mode["adapter"]), cstr(mode["description"]), str(mode["width"]),
                  str(mode["height"]), str(mode["depth"]), str(mode["dpi"]),
                  f"ops{index}", str(len(mode["ops"]))]
        code.append("    {" + ",".join(values) + "},")
    code.extend(["};", "#define MODE_COUNT (sizeof(modes)/sizeof(modes[0]))",
                 "static const PvFile payloads[] = {"])
    for (adapter, name, target), size in sorted(files.items()):
        code.append(f"    {{{cstr(adapter)},{cstr(name)},{1 if target == 'SYSTEM' else 0},{size}UL}},")
    code.extend(["};", "#define FILE_COUNT (sizeof(payloads)/sizeof(payloads[0]))", ""])
    return "\n".join(code), modes, provenance


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("drivers", type=Path, help="Directory containing S3 and ET4")
    parser.add_argument("--output", type=Path, default=ROOT / "output/windows-video")
    parser.add_argument("--edid", type=Path, default=ROOT / "dac-crt.bin")
    args = parser.parse_args()
    edid = args.edid.read_bytes()
    allowed = edid_resolutions(edid)
    args.output.mkdir(parents=True, exist_ok=True)
    if (args.output / "DISK").resolve().is_relative_to(args.drivers.resolve()):
        parser.error("Output must be outside the supplied driver directory")
    if (args.output / "DISK").exists():
        shutil.rmtree(args.output / "DISK")
    code, modes, sources = make_catalog(args.drivers, args.output / "DISK", allowed)
    if {m["adapter"] for m in modes} != {"S3", "ET4"}:
        parser.error("Both adapters must have EDID-compatible modes")
    (args.output / "catalog.h").write_text(code)
    manifest = dict(edid_sha256=hashlib.sha256(edid).hexdigest(),
                    edid_resolutions=sorted(allowed), modes=modes, source_sha256=sources)
    (args.output / "catalog.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Prepared {len(modes)} modes: " + ", ".join(
        f"{w}x{h}" for w, h in sorted({(m['width'], m['height']) for m in modes})))


if __name__ == "__main__":
    main()
