from array import array
import importlib.util
from pathlib import Path
import struct
import sys
import unittest


path = Path(__file__).resolve().parents[2] / "Native/tools/texture_bank.py"
spec = importlib.util.spec_from_file_location("texture_bank", path)
texture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(texture)


class TextureDecodeTests(unittest.TestCase):
    def test_direct_layouts_match_cabinet_address_and_color_conversion(self):
        for width, height in ((256, 256), (32, 8), (8, 32)):
            words = array("H", range(width * height))
            if sys.byteorder != "little":
                words.byteswap()
            for layout in ("linear", "twiddled"):
                for fmt in range(3):
                    expected = b"".join(
                        bytes(texture.expand(texture.texel_index(x, y, width, height, layout), fmt))
                        for y in range(height) for x in range(width))
                    self.assertEqual(texture.decode(words.tobytes(), width, height, fmt, layout), expected)

    def test_vq_block_and_pixel_order_on_rectangular_banks(self):
        for width, height in ((8, 32), (32, 8)):
            bw, bh = width // 2, height // 2
            book = struct.pack("<1024H", *[(word * 59) & 0xffff for word in range(1024)])
            indices = bytes((i * 33) % 256 for i in range(bw * bh))
            for fmt in range(3):
                expected = bytearray()
                for y in range(height):
                    for x in range(width):
                        code = indices[texture.texel_index(x // 2, y // 2, bw, bh, "twiddled")]
                        index = code * 4 + (x % 2) * 2 + y % 2
                        word = struct.unpack_from("<H", book, index * 2)[0]
                        expected.extend(texture.expand(word, fmt))
                self.assertEqual(texture.decode_vq(book + indices, width, height, fmt), expected)
