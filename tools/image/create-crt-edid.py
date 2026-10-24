#!/usr/bin/env python3
"""Add DOS CRT modes without replacing any advertised DAC capabilities.

Usage: python3 tools/image/create-crt-edid.py dac-original.bin dac-crt.bin
The input must have a CTA extension with enough unused trailing DTD space.
Promote old digital EDIDs to 1.3 so Linux parses HDMI/audio capabilities.
Other existing EDID conformity problems are preserved.
"""

import argparse
from pathlib import Path


# Pixel clock in kHz; active, sync start, sync end, total; sync polarities.
# The two VGA timings use the usual 800 x 449 raster. EDID clocks have
# 10 kHz resolution, so 25.175 MHz is rounded to 25.180 MHz (70.1002 Hz).
# Hercules is a custom VGA-compatible 70 Hz adaptation, not its original
# approximately 50 Hz / 18 kHz timing. Start with VGA 720x350 timing and
# remove one active line at either edge, preserving the 900 x 449 total.
# For 640x350 at 60 Hz retain VGA's 31.5 kHz / 525-line raster rather than
# generating a roughly 22 kHz EGA scan. Use the largest DTD-encodable front
# porch (63 lines) to keep the smaller active picture close to the center.
MODES = (
    (25180, (640, 656, 752, 800), (400, 412, 414, 449), False, True),
    (25180, (640, 656, 752, 800), (350, 387, 389, 449), True, False),
    (28320, (720, 738, 846, 900), (348, 386, 388, 449), True, False),
    (25200, (640, 656, 752, 800), (350, 413, 415, 525), True, False),
)


def validate(data):
    if len(data) < 128 or len(data) % 128:
        raise ValueError("EDID length must be a nonzero multiple of 128 bytes")
    if data[:8] != bytes.fromhex("00 ff ff ff ff ff ff 00"):
        raise ValueError("Invalid EDID header")
    if len(data) != 128 * (data[126] + 1):
        raise ValueError("Extension count does not match the file length")
    for offset in range(0, len(data), 128):
        if sum(data[offset:offset + 128]) % 256:
            raise ValueError(f"Invalid checksum in block {offset // 128}")


def encode_dtd(mode, width_mm, height_mm):
    clock, horizontal, vertical, hpositive, vpositive = mode
    ha, hs, he, ht = horizontal
    va, vs, ve, vt = vertical
    hb, hf, hw = ht - ha, hs - ha, he - hs
    vb, vf, vw = vt - va, vs - va, ve - vs
    dtd = bytearray(18)
    dtd[:2] = (clock // 10).to_bytes(2, "little")
    dtd[2:5] = bytes((ha & 255, hb & 255, (ha >> 8) << 4 | hb >> 8))
    dtd[5:8] = bytes((va & 255, vb & 255, (va >> 8) << 4 | vb >> 8))
    dtd[8:12] = bytes((hf & 255, hw & 255, (vf & 15) << 4 | (vw & 15),
                       (hf >> 8) << 6 | (hw >> 8) << 4 | (vf >> 4) << 2 | vw >> 4))
    dtd[12:15] = bytes((width_mm & 255, height_mm & 255,
                        (width_mm >> 8) << 4 | height_mm >> 8))
    # Digital separate sync, progressive. No DRM hardware doublescan.
    dtd[17] = 0x18 | (int(hpositive) << 1) | (int(vpositive) << 2)
    return bytes(dtd)


def extend_edid(original):
    validate(original)
    if original[18] != 1:
        raise ValueError("Only EDID version 1 is supported")
    if not original[20] & 0x80:
        raise ValueError("The HDMI DAC EDID must describe a digital input")
    descriptors = b"".join(encode_dtd(mode, original[21] * 10, original[22] * 10)
                           for mode in MODES)
    for block_offset in range(128, len(original), 128):
        block = original[block_offset:block_offset + 128]
        if block[0] != 0x02 or block[1] < 3 or not 4 <= block[2] <= 126:
            continue
        for slot in range(block[2], 127 - len(descriptors) + 1, 18):
            # Only use an entirely empty tail. Never overwrite a descriptor,
            # data block, native-mode flag, or any original timing.
            if any(block[slot:127]):
                continue
            offset = block_offset + slot
            result = bytearray(original)
            # Linux update_display_info() skips CTA HDMI/audio parsing for
            # revisions below 1.3; VC4 then treats the sink as DVI and refuses
            # audio. Clear input bits reserved for digital displays in 1.3.
            if result[19] < 3:
                result[19] = 3
                result[20] &= 0x81
                result[127] = (-sum(result[:127])) & 255
            result[offset:offset + len(descriptors)] = descriptors
            checksum = block_offset + 127
            result[checksum] = (-sum(result[block_offset:checksum])) & 255
            validate(result)
            allowed = set(range(offset, offset + len(descriptors))) | {19, 20, 127, checksum}
            if any(a != b and i not in allowed
                   for i, (a, b) in enumerate(zip(original, result))):
                raise ValueError("Unexpected modification outside timings and HDMI metadata")
            return bytes(result), offset
    raise ValueError(f"No CTA extension has {len(descriptors)} bytes of unused trailing DTD space")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("original", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    if args.original.resolve() == args.output.resolve():
        parser.error("Keep the original EDID: select a different output file")
    try:
        result, offset = extend_edid(args.original.read_bytes())
    except (OSError, ValueError) as error:
        parser.exit(1, f"Error: {error}\n")
    args.output.write_bytes(result)
    print(f"Wrote {args.output}: {len(result)} bytes, all block checksums valid")
    print(f"Digital EDID version: {result[18]}.{result[19]} (HDMI/audio parsing requires 1.3+)")
    print(f"Added {len(MODES)} DTDs at offset 0x{offset:02x}; original capabilities preserved")
    for clock, h, v, hp, vp in MODES:
        print(f"  {h[0]}x{v[0]}: {clock * 1000 / h[3] / v[3]:.6f} Hz, "
              f"{clock / h[3]:.6f} kHz, "
              f"{'+' if hp else '-'}HSync {'+' if vp else '-'}VSync")


if __name__ == "__main__":
    main()
