"""Export the player's installed car and HUD artwork for editing."""
import argparse
import json
from pathlib import Path
import struct
import sys
import zlib

ROOT = Path(__file__).resolve().parents[2]


def banks(root, category):
    art = root / "InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets"
    folder = art / {"Cars": "cars", "HUD": "hud"}[category]
    result = {p.parent.parent.name: p for p in folder.glob("*/textures/textures.idastex")}
    sprites = art / "menus/game2d/textures/textures.idastex"
    if category == "HUD":
        if sprites.is_file():
            result["game2d_sprites"] = sprites
        for bank in (art / "navigation").glob("*/textures.idastex"):
            result["navigation/" + bank.parent.name] = bank
        for bank in (folder / "faces").glob("*/textures/textures.idastex"):
            result["faces/" + bank.parent.parent.name] = bank
    if not result:
        raise ValueError("Launch the game and finish disc setup first.")
    return result


def textures(bank):
    with bank.open("rb") as source:
        if source.read(12) != b"IDAS3T1\0" + struct.pack("<I", 1):
            raise ValueError(f"Unsupported texture bank: {bank}")
        count, = struct.unpack("<I", source.read(4))
        if not 0 < count <= 4096:
            raise ValueError("Invalid texture count.")
        seen = set()
        for _ in range(count):
            index, width, height, size = struct.unpack("<4I", source.read(16))
            if (index >= count or index in seen or not 0 < width <= 2048 or
                    not 0 < height <= 2048 or size != width * height * 4):
                raise ValueError("Invalid texture dimensions or index.")
            rgba = source.read(size)
            if len(rgba) != size:
                raise ValueError("The texture bank is incomplete.")
            seen.add(index)
            yield index, width, height, rgba


def png(width, height, rgba):
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    stride = width * 4
    rows = b"".join(b"\0" + rgba[i:i + stride] for i in range(0, len(rgba), stride))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">2I5B", width, height, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def write_new(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        output = path.open("xb")
    except FileExistsError:
        return False
    try:
        with output:
            output.write(content)
    except OSError:
        path.unlink(missing_ok=True)
        raise
    return True


def export(root, category, name):
    catalog = banks(root, category)
    if name not in catalog:
        raise ValueError(f"Unknown {category} bank. Choose one listed by the workshop.")
    work = root / "Mods/Workshop" / category / name
    active = root / "Mods" / category / name
    active.mkdir(parents=True, exist_ok=True)
    added = 0
    for index, width, height, rgba in textures(catalog[name]):
        dest = work / f"texture_{index:03}.png"
        if not dest.exists():
            added += write_new(dest, png(width, height, rgba))
    print(f"Exported {added} new PNGs to {work}\nEdit a PNG, copy it to {active}, then restart the game.")


def demo(root):
    # OriginalRaceHud rotates chunk 191. Texture 53 supplies its red needle.
    bank = banks(root, "HUD").get("game2d")
    if bank is None:
        raise ValueError("The original game2d HUD bank is missing. Verify the installed disc assets.")
    for index, width, height, rgba in textures(bank):
        if index != 53:
            continue
        pixels = bytearray(rgba)
        for i in range(0, len(pixels), 4):
            red, green, blue, alpha = pixels[i:i + 4]
            if alpha and red > max(green, blue) + 20:
                pixels[i:i + 3] = bytes((blue, red, red))
        if pixels == rgba:
            raise ValueError("This bank does not contain the expected red needle.")
        path = root / "Mods/HUD/game2d/texture_053.png"
        if not write_new(path, png(width, height, pixels)):
            raise ValueError(f"A skin already exists at {path}. Move it aside to try the example.")
        print(f"Installed the cyan tachometer needle. Restart and use the original meter.\n"
              f"To restore its original color, remove {path} and restart.")
        return
    raise ValueError("The original meter texture is missing.")


def install_shortcut(root):
    # An existing player's palette belongs to them, including across updates.
    keys = ("timer", "meter", "records", "mirrorFrame", "minimap", "legendOpponent",
            "onlineOpponent", "timeExtended")
    palette = {key: "#FFFFFF" for key in keys}
    write_new(root / "Mods/HUD/Race Colors.json", (json.dumps(palette, indent=2) + "\n").encode("utf-8"))
    launcher = root / "Mods/Texture Workshop.cmd"
    command = ('@echo off\r\nsetlocal\r\n'
               'set "tools=%~dp0..\\InitialDUnity_Data\\GameSetup"\r\n'
               '"%tools%\\python\\python.exe" -B "%tools%\\skin_workshop.py" %*\r\n'
               'set "result=%errorlevel%"\r\nif "%~1"=="" pause\r\nexit /b %result%\r\n')
    if not write_new(launcher, command.encode("ascii")):
        # Refresh a generated shortcut while preserving any custom commands.
        import hashlib
        if hashlib.sha256(launcher.read_bytes()).hexdigest() == "41ff7b4e0aebd81d9da6dbe0cac528b23a706010e6b4ef1edb9246a6e57d3146":
            from disc_portable import write_text
            write_text(launcher, command)


def interactive(root):
    print("1  Export car textures\n2  Export in-race HUD textures\n3  Try a cyan tachometer needle\nEnter to close")
    choice = input("> ").strip()
    if not choice:
        return
    if choice == "3":
        demo(root)
        return
    if choice not in ("1", "2"):
        raise ValueError("Choose 1, 2 or 3.")
    category = "Cars" if choice == "1" else "HUD"
    names = sorted(banks(root, category))
    for number, name in enumerate(names, 1):
        description = {
            "game2d": "timer, section times, original tachometer, race messages",
            "game2d_sprites": "speed digits, transmission and gear",
            "game2d_ta": "Time Attack record and difference panels",
            "cupwater": "paper cup",
        }.get(name, "")
        if name.startswith("navigation/"):
            description = "course minimap, frame and driver markers"
        elif name.startswith("faces/"):
            description = "rival portrait"
        print(f"{number:2}. {name}" + (f"  ({description})" if description else ""))
    answer = input("Bank number or name: ").strip()
    name = names[int(answer) - 1] if answer.isdecimal() and 1 <= int(answer) <= len(names) else answer
    export(root, category, name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    actions = parser.add_subparsers(dest="action")
    for action in ("list", "export"):
        command = actions.add_parser(action)
        command.add_argument("category", choices=("Cars", "HUD"))
        if action == "export":
            command.add_argument("bank")
    actions.add_parser("demo")
    args = parser.parse_args()
    try:
        if args.action == "list":
            print("\n".join(sorted(banks(ROOT, args.category))))
        elif args.action == "export":
            export(ROOT, args.category, args.bank)
        elif args.action == "demo":
            demo(ROOT)
        else:
            interactive(ROOT)
    except (ValueError, OSError, struct.error, EOFError) as error:
        sys.exit(str(error))
