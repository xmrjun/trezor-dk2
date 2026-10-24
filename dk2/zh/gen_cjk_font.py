#!/usr/bin/env python3
"""Generates the built-in CJK glyph tables (rust/src/ui/display/cjk/*.bin) for
the D002 (STM32U5G9J-DK2) port with the eckhart layout.

All non-ASCII characters used in the Chinese translation are rendered with
Noto Sans SC in three pixel sizes, in the same 4 bpp glyph format as the
firmware fonts (see core/tools/codegen/gen_font.py). Table layout is described
in rust/src/ui/display/cjk.rs.

Usage: gen_cjk_font.py zh.json ../../core/embed/rust/src/ui/display/cjk
"""

import json
import os
import struct
import sys
from pathlib import Path

from PIL import ImageFont

# NotoSansSC-Regular.otf / NotoSansSC-Medium.otf (SIL OFL), see dk2/README.md
FONT_DIR = Path(os.environ.get("NOTO_SC_DIR", Path(__file__).parent / "fonts"))

# (table name, font file, pixel size); sizes are ~0.9x the eckhart fonts they
# stand in for (22, 26/30, 38+), CJK glyphs fill the whole em square.
TABLES = [
    ("cjk_20", "NotoSansSC-Regular.otf", 20),
    ("cjk_23", "NotoSansSC-Medium.otf", 23),
    ("cjk_34", "NotoSansSC-Regular.otf", 34),
]


def collect_chars(zh_json: Path) -> list[str]:
    data = json.loads(zh_json.read_text(encoding="utf-8"))
    chars: set[str] = set()

    def visit(value):
        if isinstance(value, str):
            chars.update(c for c in value if ord(c) >= 0x7F)
        elif isinstance(value, dict):
            for v in value.values():
                visit(v)

    visit(data["translations"])
    # glyphs are looked up by u16 codepoint
    return sorted(c for c in chars if ord(c) <= 0xFFFF)


def pack_4bpp(pixels: list[int], width: int, height: int) -> bytes:
    # same packing as gen_font.process_bitmap_buffer(bpp=4)
    res = []
    for y in range(height):
        row = pixels[y * width : (y + 1) * width]
        for a, b in zip(row[::2], row[1::2]):
            res.append((b & 0xF0) | (a >> 4))
        if width & 1:
            res.append(row[-1] >> 4)
    return bytes(res)


def render_glyph(font: ImageFont.FreeTypeFont, ch: str) -> bytes:
    ascent, _descent = font.getmetrics()
    mask, (off_x, off_y) = font.getmask2(ch, mode="L")
    width, height = mask.size
    advance = round(font.getlength(ch))
    bearing_x = max(off_x, 0)
    bearing_y = max(ascent - off_y, 0)
    for value in (width, height, advance, bearing_x, bearing_y):
        assert 0 <= value <= 255, (ch, value)
    header = bytes([width, height, advance, bearing_x, bearing_y])
    if width == 0 or height == 0:
        return header
    return header + pack_4bpp(list(mask), width, height)


def build_table(chars: list[str], font_file: str, size: int) -> bytes:
    font = ImageFont.truetype(str(FONT_DIR / font_file), size)
    glyphs = [render_glyph(font, ch) for ch in chars]

    entries = b""
    offset = 0
    for ch, glyph in zip(chars, glyphs):
        entries += struct.pack("<HHI", ord(ch), 0, offset)
        offset += len(glyph)
    return b"CJK1" + struct.pack("<HH", len(chars), 0) + entries + b"".join(glyphs)


def main() -> None:
    zh_json, out_dir = Path(sys.argv[1]), Path(sys.argv[2])
    out_dir.mkdir(parents=True, exist_ok=True)
    chars = collect_chars(zh_json)
    print(f"{len(chars)} characters")
    total = 0
    for name, font_file, size in TABLES:
        table = build_table(chars, font_file, size)
        (out_dir / f"{name}.bin").write_bytes(table)
        total += len(table)
        print(f"{name}.bin: {len(table)} bytes")
    print(f"total: {total} bytes")


if __name__ == "__main__":
    main()
