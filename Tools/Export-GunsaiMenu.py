"""Add the course map to Gunsai's menu card and rebuild RuntimeAssets/GUNSAI/menu.idastex.

The card background (with title) is RuntimeAssets/GUNSAI/menu-preview.png; the
map follows Export-SadamineMenu.ps1: timed route only, -x right, +z up.
The 96x64 carousel tile is cropped from thumbnail-preview.png with the same
badge and title treatment as the other imported courses.
"""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

START, GOAL = 93, 3094  # race.bin checkpoints
SS = 4  # supersampling for antialiased lines


def write_bank(path, images):
    out = [b"IDAS3T1\0", struct.pack("<II", 1, len(images))]
    for index, image in enumerate(images):
        rgba = image.convert("RGBA").tobytes()
        out += [struct.pack("<4I", index, image.width, image.height, len(rgba)), rgba]
    path.write_bytes(b"".join(out))


def road_centre(path):
    data = path.read_bytes()
    count = struct.unpack_from("<I", data, 4)[0]
    return [struct.unpack_from("<3f", data, 8 + 12 * i) for i in range(count)]


def label(draw, text, x, y, font):
    draw.text((x * SS, y * SS), text, font=font, fill="white",
              stroke_width=2 * SS, stroke_fill="black")


def make_tile(source, badge, colour):
    # Centre-crop to the tile's 3:2 aspect rather than squashing the shot.
    width = min(source.width, source.height * 3 // 2)
    height = width * 2 // 3
    left, top = (source.width - width) // 2, (source.height - height) // 2
    tile = source.convert("RGBA").crop((left, top, left + width, top + height)).resize((96 * SS, 64 * SS), Image.LANCZOS)
    draw = ImageDraw.Draw(tile)
    badge_font = ImageFont.truetype("arialbd.ttf", 11 * SS)
    draw.rectangle([0, 0, (draw.textlength(badge, font=badge_font) / SS + 3) * SS, 14 * SS], fill=colour)
    draw.text((1 * SS, 0), badge, font=badge_font, fill="white")
    label(draw, "Gunsai", 6, 36, ImageFont.truetype("timesbi.ttf", 19 * SS))
    return tile.resize((96, 64), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", default=Path(__file__).resolve().parents[1] / "RuntimeAssets" / "GUNSAI", type=Path)
    parser.add_argument("--badge", default="NORMAL")
    parser.add_argument("--badge-colour", default="green")
    args = parser.parse_args()
    background = Image.open(args.pack / "menu-preview.png").convert("RGBA")
    if background.size != (640, 312):
        raise ValueError("Gunsai menu card must be 640x312")
    thumbnail = Image.open(args.pack / "thumbnail-preview.png")
    tile = make_tile(thumbnail, args.badge, args.badge_colour)

    centre = road_centre(args.pack / "road.bin")
    points = [(x, -z) for x, _, z in centre[START:GOAL + 1:8]] + [(centre[GOAL][0], -centre[GOAL][2])]
    min_x, max_x = min(p[0] for p in points), max(p[0] for p in points)
    min_y = min(p[1] for p in points)
    max_y = max(p[1] for p in points)
    scale = min(165 / (max_x - min_x), 228 / (max_y - min_y))
    line = [((435 + (max_x - x) * scale) * SS, (42 + (y - min_y) * scale) * SS) for x, y in points]

    overlay = Image.new("RGBA", (640 * SS, 312 * SS))
    draw = ImageDraw.Draw(overlay)
    draw.line(line, fill=(0, 0, 0, 150), width=5 * SS, joint="curve")
    draw.line(line, fill="white", width=2 * SS, joint="curve")
    for x, y in (line[0], line[-1]):
        draw.ellipse([x - 3 * SS, y - 3 * SS, x + 3 * SS, y + 3 * SS], fill="white")
    font = ImageFont.truetype("timesbi.ttf", 14 * SS)
    (sx, sy), (ex, ey) = [(x / SS, y / SS) for x, y in (line[0], line[-1])]
    outbound = draw.textlength("OUTBOUND", font=font) / SS
    label(draw, "OUTBOUND", sx - outbound - 6, sy - 8, font)
    label(draw, "INBOUND", ex + 6, ey - 8, font)

    card = Image.alpha_composite(background, overlay.resize((640, 312), Image.LANCZOS))
    card.save(args.pack / "menu-card.png")
    tile.save(args.pack / "menu-tile.png")
    write_bank(args.pack / "menu.idastex", [card, tile])


if __name__ == "__main__":
    main()
