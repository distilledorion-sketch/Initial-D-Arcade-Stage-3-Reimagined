import struct
import tempfile
import unittest
import zlib
from pathlib import Path

import skin_workshop


class SkinWorkshopTests(unittest.TestCase):
    def test_export_example_and_existing_edits(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bank = root / "InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets/hud/game2d/textures/textures.idastex"
            bank.parent.mkdir(parents=True)
            pixels = bytes((190, 21, 13, 64, 7, 5, 4, 128))
            bank.write_bytes(b"IDAS3T1\0" + struct.pack("<II", 1, 54) + b"".join(
                struct.pack("<4I", i, 2, 1, len(pixels)) + pixels for i in range(54)))
            source = bank.read_bytes()
            skin_workshop.export(root, "HUD", "game2d")
            work = root / "Mods/Workshop/HUD/game2d/texture_053.png"
            active = root / "Mods/HUD/game2d/texture_053.png"
            self.assertFalse(active.exists())
            def row(path):
                encoded = path.read_bytes()
                length, = struct.unpack_from(">I", encoded, 33)
                self.assertEqual(encoded[37:41], b"IDAT")
                return zlib.decompress(encoded[41:41 + length])
            self.assertEqual(row(work), b"\0" + pixels)
            work.write_bytes(b"my edited artwork")
            skin_workshop.export(root, "HUD", "game2d")
            self.assertEqual(work.read_bytes(), b"my edited artwork")
            skin_workshop.demo(root)
            self.assertEqual(row(active), bytes((0, 13, 190, 190, 64, 7, 5, 4, 128)))
            active.write_bytes(b"another skin")
            with self.assertRaises(ValueError):
                skin_workshop.demo(root)
            self.assertEqual(active.read_bytes(), b"another skin")
            self.assertEqual(bank.read_bytes(), source)

    def test_race_banks_and_player_palette(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            art = root / "InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets"
            for bank in ("navigation/ez/textures.idastex", "hud/faces/face_bunt/textures/textures.idastex"):
                path = art / bank
                path.parent.mkdir(parents=True)
                path.touch()
            self.assertEqual(set(skin_workshop.banks(root, "HUD")), {"navigation/ez", "faces/face_bunt"})
            palette = root / "Mods/HUD/Race Colors.json"
            skin_workshop.install_shortcut(root)
            self.assertIn('"timer": "#FFFFFF"', palette.read_text())
            palette.write_text('{"timer":"#EE8800"}')
            skin_workshop.install_shortcut(root)
            self.assertEqual(palette.read_text(), '{"timer":"#EE8800"}')

    def test_export_requires_a_listed_bank(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            bank = root / "InitialDUnity_Data/StreamingAssets/IDAS3/data/original_assets/cars/car/textures"
            bank.mkdir(parents=True)
            (bank / "textures.idastex").touch()
            with self.assertRaises(ValueError):
                skin_workshop.export(root, "Cars", "../outside")
            self.assertFalse((root / "Mods").exists())
