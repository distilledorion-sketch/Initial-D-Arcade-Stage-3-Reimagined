"""Build Odawara's menu card and tile and RuntimeAssets/ODAWARA/menu.idastex.

Both are cut from RuntimeAssets/ODAWARA/thumbnail-preview.png, a view of the
start line (Tools/Render-ImportedScenePreview.py, or an in-game capture). The
card follows Export-GunsaiMenu.py: title, rule and course map with -x right
and +z up. Odawara is a circuit, so the map is the whole counterclockwise lap
and marks its start/finish line.
"""
import argparse
import struct
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

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


def crop(source, width, height):
    """Centre-crop to the target aspect rather than squashing the shot."""
    w = min(source.width, source.height * width // height)
    h = w * height // width
    left, top = (source.width - w) // 2, (source.height - h) // 2
    return source.convert("RGBA").crop((left, top, left + w, top + h))


def make_tile(source, badge, colour):
    tile = crop(source, 96, 64).resize((96 * SS, 64 * SS), Image.LANCZOS)
    draw = ImageDraw.Draw(tile)
    badge_font = ImageFont.truetype("arialbd.ttf", 11 * SS)
    draw.rectangle([0, 0, (draw.textlength(badge, font=badge_font) / SS + 3) * SS, 14 * SS], fill=colour)
    draw.text((1 * SS, 0), badge, font=badge_font, fill="white")
    label(draw, "Odawara", 4, 36, ImageFont.truetype("timesbi.ttf", 19 * SS))
    return tile.resize((96, 64), Image.LANCZOS)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--pack", default=Path(__file__).resolve().parents[1] / "RuntimeAssets" / "ODAWARA", type=Path)
    parser.add_argument("--badge", default="NORMAL")
    parser.add_argument("--badge-colour", default="green")
    args = parser.parse_args()
    thumbnail = Image.open(args.pack / "thumbnail-preview.png")
    tile = make_tile(thumbnail, args.badge, args.badge_colour)
    background = crop(thumbnail, 640, 312).resize((640, 312), Image.LANCZOS)

    # The path repeats its first point, so the line closes on the start.
    centre = road_centre(args.pack / "road.bin")
    points = [(x, -z) for x, _, z in centre[::8]] + [(centre[0][0], -centre[0][2])]
    min_x, max_x = min(p[0] for p in points), max(p[0] for p in points)
    min_y, max_y = min(p[1] for p in points), max(p[1] for p in points)
    scale = min(165 / (max_x - min_x), 228 / (max_y - min_y))
    line = [((435 + (max_x - x) * scale) * SS, (42 + (y - min_y) * scale) * SS) for x, y in points]

    overlay = Image.new("RGBA", (640 * SS, 312 * SS))
    draw = ImageDraw.Draw(overlay)
    draw.line([(0, 119 * SS), (640 * SS, 119 * SS)], fill=(0, 0, 0, 150), width=SS)
    draw.line([(0, 118 * SS), (640 * SS, 118 * SS)], fill="white", width=SS)
    draw.text((42 * SS, 85 * SS), "Odawara", font=ImageFont.truetype("timesbi.ttf", 64 * SS), anchor="ls",
              fill="black", stroke_width=2 * SS, stroke_fill="white")
    draw.line(line, fill=(0, 0, 0, 150), width=5 * SS, joint="curve")
    draw.line(line, fill="white", width=2 * SS, joint="curve")
    x, y = line[0]
    draw.ellipse([x - 3 * SS, y - 3 * SS, x + 3 * SS, y + 3 * SS], fill="white")
    font = ImageFont.truetype("timesbi.ttf", 14 * SS)
    label(draw, "START", x / SS - draw.textlength("START", font=font) / SS - 6, y / SS - 8, font)

    card = Image.alpha_composite(background, overlay.resize((640, 312), Image.LANCZOS))
    card.save(args.pack / "menu-card.png")
    tile.save(args.pack / "menu-tile.png")
    write_bank(args.pack / "menu.idastex", [card, tile])


if __name__ == "__main__":
    main()
