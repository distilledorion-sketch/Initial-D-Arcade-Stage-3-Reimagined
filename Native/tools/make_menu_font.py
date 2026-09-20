"""Bakes the menu font atlas the native save-file screen draws with.

The recovered cabinet alphabet is a display face with no lowercase, no comma
and no slash, and it does not suit a dense list of files. This bakes a plain
variable-width atlas instead: white glyphs on transparent, with the metrics
beside them, so the C++ side can place and tint them itself.

    python make_menu_font.py --project <native root>
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

FIRST, LAST = 32, 126
CELL = 48          # baked height; the game samples this down to whatever it needs
PAD = 2


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--project', type=Path, required=True)
    p.add_argument('--face', default='C:/Windows/Fonts/bahnschrift.ttf')
    p.add_argument('--weight', type=float, default=650)
    a = p.parse_args()

    font = ImageFont.truetype(a.face, CELL)
    try:
        font.set_variation_by_axes([a.weight])
    except Exception:
        pass

    glyphs = []
    for code in range(FIRST, LAST + 1):
        ch = chr(code)
        box = font.getbbox(ch)
        advance = font.getlength(ch)
        width = max(1, int(round(box[2] - box[0])) + PAD * 2) if box else 1
        glyphs.append((code, ch, width, advance, box))

    total = sum(g[2] for g in glyphs)
    columns = 24
    rows = (len(glyphs) + columns - 1) // columns
    cellWidth = max(g[2] for g in glyphs)
    atlas = Image.new('RGBA', (columns * cellWidth, rows * (CELL + PAD * 2)), (255, 255, 255, 0))
    draw = ImageDraw.Draw(atlas)

    metrics = []
    ascent, _ = font.getmetrics()
    for index, (code, ch, width, advance, box) in enumerate(glyphs):
        cx = (index % columns) * cellWidth
        cy = (index // columns) * (CELL + PAD * 2)
        # Draw from a common baseline so every glyph shares one vertical origin.
        draw.text((cx + PAD - (box[0] if box else 0), cy + PAD), ch, font=font, fill=(255, 255, 255, 255))
        metrics.append((code, cx, cy, width, CELL + PAD * 2, advance))

    out = a.project / 'data/native_assets/menu_font'
    out.mkdir(parents=True, exist_ok=True)
    atlas.save(out / 'font.png')          # kept for inspection only
    # The project has no PNG decoder, so the asset itself is a raw ARGB blob
    # that carries its own metrics.
    import struct
    pixels = atlas.load()
    blob = bytearray(b'IDMF')
    blob += struct.pack('<5I', 1, atlas.width, atlas.height, CELL, len(metrics))
    for code, x, y, w, h, advance in metrics:
        blob += struct.pack('<I5f', code, float(x), float(y), float(w), float(h), float(advance))
    for yy in range(atlas.height):
        for xx in range(atlas.width):
            r, g, b, al = pixels[xx, yy]
            blob += struct.pack('<I', (al << 24) | (r << 16) | (g << 8) | b)
    (out / 'font.bin').write_bytes(bytes(blob))
    print(f'wrote {out} : {atlas.width}x{atlas.height}, {len(metrics)} glyphs, {len(blob)} bytes')


if __name__ == '__main__':
    main()
