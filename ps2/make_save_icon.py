#!/usr/bin/env python3
"""Creates the memory card icon files of the PS2 port: ps2/zelda3.icn and ps2/icon.sys (embedded into the ELF).

usage:  python3 ps2/make_save_icon.py [picture]
  picture  any image; it is scaled to 128x128. Without one a Triforce is drawn.
Needs numpy and Pillow. The two files are copied to the save folder on the memory card at the first save.
A zelda3.icn / icon.sys placed next to the game (zelda3.elf) replace the built-in ones without rebuilding.
"""
import struct
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

TITLE = ("ZELDA 3", "A LINK TO THE PAST")
ICON_NAME = b"zelda3.icn"


def default_picture():
    im = Image.new("RGBA", (128, 128))
    d = ImageDraw.Draw(im)
    for y in range(128):  # dark green gradient
        d.line([(0, y), (127, y)], fill=(0, 40 - y // 8, 24 - y // 12, 255))
    gold, edge = (248, 208, 48, 255), (255, 244, 160, 255)
    for tri in ([(64, 14), (41, 58), (87, 58)], [(39, 66), (16, 110), (62, 110)], [(89, 66), (66, 110), (112, 110)]):
        d.polygon(tri, fill=gold, outline=edge)
    return im


def texture(im):
    a = np.asarray(im.convert("RGBA").resize((128, 128), Image.LANCZOS), np.uint32)
    r, g, b, al = (a[..., i] >> 3 for i in range(4))
    return (r | g << 5 | b << 10 | (al >> 4) << 15).astype("<u2").tobytes()


def icn(tex):  # one flat textured quad (two triangles), about the size of a browser icon
    s, top, bot = 7000, -14500, -500
    quad = [(-s, top, 0, 0), (s, top, 4096, 0), (-s, bot, 0, 4096), (s, top, 4096, 0), (s, bot, 4096, 4096), (-s, bot, 0, 4096)]
    out = struct.pack("<IIIfI", 0x10000, 1, 7, 1.0, len(quad))
    for x, y, u, v in quad:
        out += struct.pack("<4h4h2h4B", x, y, 0, 0x400, 0, 0, -4096, 0, u, v, 0x80, 0x80, 0x80, 0x80)
    out += struct.pack("<IIfII", 1, 1, 1.0, 0, 1) + struct.pack("<IIff", 0, 1, 0.0, 1.0)
    return out + tex


def sjis(s):  # the browser wants Shift-JIS; ASCII is shown as full-width characters
    return "".join("\u3000" if c == " " else chr(ord(c) + 0xFEE0) for c in s).encode("shift_jis")


def icon_sys(icon_name):
    title = sjis(TITLE[0]) + sjis(TITLE[1])
    out = b"PS2D" + struct.pack("<HHII", 0, len(sjis(TITLE[0])), 0, 0x60)
    out += struct.pack("<16I", *[0x00, 0x30, 0x18, 0] * 2 + [0x00, 0x10, 0x08, 0] * 2)           # background colors, top to bottom
    out += struct.pack("<12f", 0.5, 0.5, 0.5, 0, 0, -0.4, -0.1, 0, -0.5, -0.5, 0.5, 0)           # light directions
    out += struct.pack("<12f", 0.3, 0.3, 0.3, 0, 0.2, 0.2, 0.2, 0, 0.1, 0.1, 0.1, 0)             # light colors
    out += struct.pack("<4f", 0.7, 0.7, 0.7, 0)                                                   # ambient: the flat picture mostly unlit
    out += title.ljust(68, b"\0") + icon_name.ljust(64, b"\0") * 3 + bytes(512)
    assert len(out) == 964 and len(title) <= 68
    return out


if __name__ == "__main__":
    here = Path(__file__).resolve().parent
    pic = Image.open(sys.argv[1]) if len(sys.argv) > 1 else default_picture()
    data = icn(texture(pic))
    assert len(data) == 20 + 6 * 24 + 20 + 16 + 128 * 128 * 2
    (here / "zelda3.icn").write_bytes(data)
    (here / "icon.sys").write_bytes(icon_sys(ICON_NAME))
    print("zelda3.icn", len(data), "bytes, icon.sys 964 bytes")
