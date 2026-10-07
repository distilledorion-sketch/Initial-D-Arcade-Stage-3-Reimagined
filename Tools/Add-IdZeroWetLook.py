"""Give an IDZero course pack an improvised wet presentation.

IDZero 2.20 has rain scenery only for Akina; Gunsai and Odawara were authored
dry. Their sky domes use the same panorama layout as Stage 8's, so wet races
take Tsubaki Line's Stage 8 rain skies, and the loader regrades the dry scenery
the way Stage 8's own wet variants differ from their dry ones:

  * baked vertex colours ~0.74-0.88 as bright, with their blue/green cast
    largely gone (Hakone and Tsubaki, course/mountain/tree means);
  * textures ~0.94 as bright (Tsubaki);
  * pale fog matching the overcast horizon, complete ~300 units out (their
    distance scales give ranges of 160-222 against 256-769 dry);
  * no hard sun shadows.

Each scenery folder gets `wet-sky.dds` and `wet.json`; no geometry or other
texture is duplicated. Idas8HakoneCourse applies them when the race is wet.

    python Tools/Add-IdZeroWetLook.py RuntimeAssets/GUNSAI RuntimeAssets/ODAWARA
"""
import argparse
import json
import shutil
import struct
from pathlib import Path

from PIL import Image

RUNTIME = Path(__file__).resolve().parents[1] / "RuntimeAssets"
# (scenery folder, donor folder, vertex colour brightness, desaturation, sun shadow strength)
VARIANTS = (("", "day_wet", 0.8, 0.55, 0.3),
            # Stage 8's night scenery has the same baked colours wet and dry.
            ("night_dry", "night_wet", 1.0, 0.0, 1.0))
FOG_RANGE = 220.0


def sky_material(folder: Path) -> dict:
    """The dome: the sky material with the largest texture (not a star layer)."""
    def pixels(material: dict) -> int:
        height, width = struct.unpack_from("<2I", (folder / material["textures"][0]["file"]).read_bytes(), 12)
        return width * height
    skies = [m for m in json.loads((folder / "scene.json").read_text())["materials"] if m["sky"] and m["textures"]]
    if not skies:
        raise ValueError(f"No sky material in {folder}")
    return max(skies, key=pixels)


def add(pack: Path, donor: Path) -> None:
    for scenery, donor_variant, brightness, desaturate, shadow in VARIANTS:
        folder = pack / scenery
        source = donor / donor_variant
        texture = source / sky_material(source)["textures"][0]["file"]
        shutil.copyfile(texture, folder / "wet-sky.dds")
        # Distant scenery fades to the sky just above the horizon (v ~0.75-0.84).
        image = Image.open(texture).convert("RGB")
        band = image.crop((0, int(image.height * .75), image.width, int(image.height * .84))).resize((1, 1), Image.BOX)
        look = {"skyMaterial": sky_material(folder)["name"], "skyTexture": "wet-sky.dds",
                "fogColor": [round(c / 255, 4) for c in band.getpixel((0, 0))], "fogRange": FOG_RANGE,
                "brightness": brightness, "desaturate": desaturate, "shadow": shadow,
                "source": f"Initial D Arcade Stage 8 / {donor.name} {donor_variant} sky; regrade measured from Stage 8 wet scenery"}
        (folder / "wet.json").write_text(json.dumps(look, indent=2) + "\n")
        print(f"{folder}: {look['skyMaterial']} <- {donor.name}/{donor_variant}/{texture.name}, fog {look['fogColor']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("packs", nargs="+", type=Path, help="IDZero course packs (RuntimeAssets/GUNSAI ...)")
    parser.add_argument("--donor", type=Path, default=RUNTIME / "TSUBAKI", help="Stage 8 pack supplying the rain skies")
    arguments = parser.parse_args()
    for pack in arguments.packs:
        add(pack, arguments.donor)
