"""Mock-up of a save-file selection screen in the game's own menu style.

Nothing here ships and nothing here is wired to the game. It composites the
real menu chrome the importer already extracted -- the angled header bars, the
list plate, the selection arrows and glow -- onto the source's 640x480 canvas so
the proportions, colours and furniture are the ones the cabinet actually uses,
and only the text is a stand-in.

    python mock_save_select.py --project <native root> --out <png> [--slot N]
"""
import argparse
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

CANVAS = (640, 480)
SCALE = 2

# Sampled from the game's own selection screens.
BLUE_DEEP = (14, 26, 54)
BLUE_MID = (26, 48, 92)
WHITE = (238, 242, 240)
DIM = (150, 166, 178)
ORANGE = (255, 168, 40)
YELLOW = (255, 207, 63)
PLATE = (18, 24, 34)
PLATE_ON = (30, 58, 116)

SLOTS = [
    ("01", "TAKUMI", "AE86 TRUENO", "GT-APEX", "47:12'08", "2026/09/04", 31, True),
    ("02", "AAAAA", "FD3S RX-7", "TYPE RS", "12:40'55", "2026/09/06", 9, True),
    ("03", "KEI", "BNR34 SKYLINE", "V-SPEC II", "03:57'21", "2026/09/07", 2, True),
    ("04", "-----", "", "", "--:--'--", "----/--/--", 0, False),
    ("05", "-----", "", "", "--:--'--", "----/--/--", 0, False),
]


def font(size, italic=True):
    path = "C:/Windows/Fonts/bahnschrift.ttf"
    f = ImageFont.truetype(path, size)
    try:
        f.set_variation_by_axes([700 if size > 20 else 600])
    except Exception:
        pass
    return f


def shear(image, amount=0.18):
    """The cabinet's plates and type lean; this is the same lean."""
    w, h = image.size
    return image.transform((w + int(h * amount), h), Image.AFFINE,
                           (1, amount, -amount * h, 0, 1, 0), resample=Image.BICUBIC)


def text(draw, xy, value, f, fill, outline=(6, 10, 18), weight=2):
    x, y = xy
    for dx in range(-weight, weight + 1):
        for dy in range(-weight, weight + 1):
            if dx or dy:
                draw.text((x + dx, y + dy), value, font=f, fill=outline)
    draw.text((x, y), value, font=f, fill=fill)


def load(project, bank, index):
    p = project / "data/original_assets/menus/v3" / bank / "textures" / f"texture_{index:03d}.png"
    if not p.exists():
        return None
    # The source stores these flipped; the game flips V when it draws them.
    return Image.open(p).convert("RGBA").transpose(Image.FLIP_TOP_BOTTOM)


def background(project):
    image = Image.new("RGB", CANVAS, BLUE_DEEP)
    d = ImageDraw.Draw(image)
    for y in range(CANVAS[1]):
        t = y / CANVAS[1]
        d.line([(0, y), (CANVAS[0], y)],
               fill=tuple(int(a + (b - a) * t) for a, b in zip(BLUE_MID, BLUE_DEEP)))
    glow = load(project, "v3sS00common", 11)
    if glow:
        glow = glow.resize((CANVAS[0], 190))
        image.paste(glow, (0, 40), glow)
    return image


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--project", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--slot", type=int, default=1)
    p.add_argument("--car", type=Path, help="a render from --factory-paint-preview")
    a = p.parse_args()

    image = background(a.project)
    d = ImageDraw.Draw(image)

    header = load(a.project, "v3sS00common", 3)      # blue/black angled bar
    accent = load(a.project, "v3sS00common", 12)     # red/black angled bar
    plate = load(a.project, "v3sS05cars", 4)         # dark angled list plate
    arrows = load(a.project, "v3sS00common", 10)

    if header:
        bar = header.resize((CANVAS[0], 34))
        image.paste(bar, (0, 30), bar)
    text(d, (28, 33), "SELECT A SAVE FILE", font(23), WHITE)
    text(d, (452, 40), "5 FILES", font(14), YELLOW, weight=1)

    top, row_h, gap = 92, 52, 6
    for i, (num, name, car, grade, played, date, wins, used) in enumerate(SLOTS):
        y = top + i * (row_h + gap)
        selected = (i + 1) == a.slot
        row = Image.new("RGBA", (470, row_h), (0, 0, 0, 0))
        rd = ImageDraw.Draw(row)
        rd.rectangle([0, 0, 469, row_h - 1], fill=PLATE_ON if selected else PLATE)
        if plate is not None and not selected:
            strip = plate.resize((470, row_h))
            row.alpha_composite(strip)
        rd.rectangle([0, 0, 469, row_h - 1], outline=(96, 122, 150) if selected else (44, 58, 76))
        rd.rectangle([0, 0, 4, row_h - 1], fill=ORANGE if selected else (52, 66, 84))
        # The cabinet leans its plates; match that rather than a plain box.
        leaned = shear(row, 0.16)
        image.paste(leaned, (30 - int(row_h * 0.16), y), leaned)

        tx = 46
        text(d, (tx, y + 8), num, font(30), YELLOW if used else (86, 98, 112))
        if used:
            text(d, (tx + 44, y + 4), name, font(21), WHITE)
            text(d, (tx + 44, y + 28), car, font(15), DIM, weight=1)
            text(d, (tx + 232, y + 28), grade, font(13), (128, 176, 220), weight=1)
            text(d, (tx + 330, y + 6), "TIME", font(11), DIM, weight=1)
            text(d, (tx + 330, y + 18), played, font(19), WHITE)
            text(d, (tx + 232, y + 6), f"{wins} WINS", font(12), ORANGE, weight=1)
            text(d, (tx + 44, y + 44) if False else (tx + 330, y + 40), date, font(10), (110, 126, 142), weight=1)
        else:
            text(d, (tx + 44, y + 14), "NO DATA", font(20), (96, 110, 126))

    # The car each file holds, on the right, the way the model screens show one.
    panel = (496, 92, 620, 372)
    d.rectangle(panel, fill=(10, 18, 36), outline=(60, 82, 110))
    if accent:
        cap = accent.resize((panel[2] - panel[0], 16))
        image.paste(cap, (panel[0], panel[1]), cap)
    chosen = SLOTS[a.slot - 1]
    text(d, (panel[0] + 8, panel[1] + 2), "CAR IN FILE", font(11), WHITE, weight=1)
    if chosen[7]:
        text(d, (panel[0] + 8, panel[1] + 30), chosen[2].split()[0], font(17), WHITE)
        text(d, (panel[0] + 8, panel[1] + 52), " ".join(chosen[2].split()[1:]), font(13), DIM, weight=1)
        box = (panel[0] + 8, panel[1] + 80, panel[2] - 8, panel[1] + 168)
        d.rectangle(box, fill=(16, 26, 44), outline=(48, 66, 90))
        if a.car and a.car.exists():
            # A real render from the game's own car presentation, which draws the
            # saved profile's parts and factory colour, not a placeholder.
            shot = Image.open(a.car).convert("RGB").crop((400, 250, 900, 590))
            shot = shot.resize((box[2] - box[0] - 2, box[3] - box[1] - 2))
            image.paste(shot, (box[0] + 1, box[1] + 1))
        else:
            text(d, (panel[0] + 26, panel[1] + 118), "CAR VIEW", font(12), (92, 108, 126), weight=1)
        for k, (label, value) in enumerate((("PLAYED", chosen[4]), ("WINS", str(chosen[6])),
                                            ("LAST", chosen[5]))):
            text(d, (panel[0] + 8, panel[1] + 182 + k * 30), label, font(10), DIM, weight=1)
            text(d, (panel[0] + 8, panel[1] + 194 + k * 30), value, font(15), WHITE, weight=1)
    else:
        text(d, (panel[0] + 20, panel[1] + 120), "EMPTY", font(18), (96, 110, 126))

    if arrows:
        # The sheet holds them pointing sideways; a list scrolls up and down.
        up = arrows.resize((26, 20)).rotate(90, expand=True)
        image.paste(up, (250, 74), up)
        down = up.transpose(Image.FLIP_TOP_BOTTOM)
        image.paste(down, (250, 396), down)

    # The control strip the selection screens carry along the bottom.
    d.rectangle([0, 436, 639, 479], fill=(8, 14, 28))
    d.line([(0, 436), (639, 436)], fill=(70, 96, 128))
    x = 28
    for key, action in (("Steering", "SELECT"), ("Accel.", "OK"), ("Brake", "BACK")):
        box = font(13)
        w = int(d.textlength(key, font=box)) + 14
        d.rectangle([x, 449, x + w, 469], fill=WHITE)
        d.text((x + 7, 452), key, font=box, fill=(12, 18, 28))
        text(d, (x + w + 10, 450), action, font(15), ORANGE, weight=1)
        x += w + 22 + int(d.textlength(action, font=font(15)))

    image.resize((CANVAS[0] * SCALE, CANVAS[1] * SCALE), Image.NEAREST).save(a.out)
    print(f"wrote {a.out} ({CANVAS[0] * SCALE}x{CANVAS[1] * SCALE}, source canvas {CANVAS[0]}x{CANVAS[1]})")


if __name__ == "__main__":
    main()
