"""Structural extraction checks. These do not establish rendered game parity."""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import extract_original_menus as menus


def fixture(tag_a=3, tag_b=12):
    descriptor = tag_a << 24 | tag_b << 16
    table = menus.TABLE.pack(0, 4, tag_a, tag_b)
    payload = b''.join(menus.VERTEX.pack(x, y, -0.0, u, v, 0x80ff0000, descriptor)
                       for x, y, u, v in [(10, 20, 0, 0), (14, 20, 1, 0),
                                          (10, 24, 0, 1), (14, 24, 1, 1)])
    return table, payload, [{'width': 4, 'height': 4}]


class OriginalMenuExtractionTests(unittest.TestCase):
    def test_exact_descriptor_bounds_and_negative_zero(self):
        table, payload, textures = fixture()
        sprite = menus.decode_rip(table, payload, textures)[0]
        self.assertEqual(sprite['packed_descriptor_hex'], '030c0000')
        self.assertEqual(sprite['authored_bounds_xy'], [10, 20, 14, 24])
        self.assertEqual(sprite['exact_texel_crop'], [0, 0, 4, 4])
        self.assertEqual(sprite['vertices'][0]['raw_words_hex'][2], '80000000')

    def test_native_rip_pack_preserves_original_vertex_bytes(self):
        table, payload, textures = fixture()
        pack = menus.make_ui_pack(menus.decode_rip(table, payload, textures), payload)
        self.assertEqual(pack[:16], b'IDAS3U1\0' + struct.pack('<II', 1, 1))
        self.assertEqual(struct.unpack_from('<5I', pack, 16), (0, 0, 4, 3, 12))
        self.assertEqual(pack[36:], payload)

    def test_bad_descriptors_counts_and_texture_references_fail(self):
        table, payload, textures = fixture()
        for malformed in [menus.TABLE.pack(1, 4, 3, 12), menus.TABLE.pack(0, 2, 3, 12),
                          menus.TABLE.pack(0, 4, 12, 3), menus.TABLE.pack(0, 4, 256, 12),
                          table + b'\0', b'']:
            with self.subTest(table=malformed), self.assertRaises(ValueError):
                menus.decode_rip(malformed, payload, textures)

    def test_payload_truncation_trailing_and_nan_fail(self):
        table, payload, textures = fixture()
        for malformed in [payload[:-1], payload + b'\0', struct.pack('<f', float('nan')) + payload[4:]]:
            with self.subTest(length=len(malformed)), self.assertRaises(ValueError):
                menus.decode_rip(table, malformed, textures)

    def test_fractional_texel_edges_are_not_resampled(self):
        table, payload, textures = fixture()
        raw = bytearray(payload)
        struct.pack_into('<f', raw, 12, 0.1)
        sprite = menus.decode_rip(table, raw, textures)[0]
        self.assertIsNone(sprite['exact_texel_crop'])

    def test_crop_uses_exact_rows(self):
        pixels = bytes(range(4 * 3 * 4))
        self.assertEqual(menus.crop_rgba(pixels, 4, 3, [1, 1, 3, 3]), pixels[20:28] + pixels[36:44])
        with self.assertRaises(ValueError):
            menus.crop_rgba(pixels, 4, 3, [-1, 0, 3, 3])

    def test_ambiguous_payload_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory)
            (p / 'x.bin').write_bytes(b'a')
            (p / 'x.bin.nz').write_bytes(b'b')
            with self.assertRaises(ValueError):
                menus.choose_payload(p, 'x')

    def test_v3_title_export_preserves_geometry_source(self):
        folder = ROOT / 'data/original_assets/menus/v3/adv_newtitle'
        if not folder.exists():
            self.skipTest('Private original title export unavailable')
        manifest = json.loads((folder / 'manifest.json').read_text())
        self.assertEqual(manifest['coordinate_transform'], 'none')
        self.assertEqual(manifest['uv_transform'], 'none')
        self.assertEqual(manifest['chunk_count'], 2)
        self.assertEqual(manifest['texture_count'], 2)
        raw = (folder / 'original_pol.bin').read_bytes()
        self.assertEqual(menus.digest(raw), manifest['decoded_polygon_sha256'])
        mesh = (folder / manifest['mesh_pack']).read_bytes()
        self.assertEqual(mesh[:16], b'IDAS3M1\0' + struct.pack('<II', 1, 2))
        cursor = 16
        for chunk in manifest['chunks']:
            chunk_index, source_offset, source_size, batch_count = struct.unpack_from('<4I', mesh, cursor)
            self.assertEqual(chunk_index, chunk['index'])
            cursor += 16
            self.assertEqual(mesh[cursor:cursor + 96], raw[source_offset:source_offset + 96])
            cursor += 96
            for batch in chunk['batches']:
                source, nv, ni, raw_bytes = struct.unpack_from('<4I', mesh, cursor)
                cursor += 16
                self.assertEqual(mesh[cursor:cursor + 32], raw[source:source + 32])
                cursor += 32 + 64 + nv * 44 + ni * 4
                self.assertEqual(mesh[cursor:cursor + raw_bytes], raw[source + 32:source + 32 + raw_bytes])
                self.assertEqual(menus.digest(mesh[cursor:cursor + raw_bytes]), batch['source_vertex_sha256'])
                cursor += raw_bytes
        self.assertEqual(cursor, len(mesh))

    def test_all_actual_rip_exports_preserve_vertex_stream(self):
        directory = ROOT / 'data/original_assets/menus'
        paths = list(directory.glob('*/original_rip.tbl'))
        if not paths:
            self.skipTest('Private original RIP exports unavailable')
        for table in paths:
            with self.subTest(bank=table.parent.name):
                manifest = json.loads((table.parent / 'manifest.json').read_text())
                payload = (table.parent / 'original_rip.bin').read_bytes()
                self.assertEqual(menus.digest(payload), manifest['source_rip_payload_sha256'])
                self.assertEqual((table.parent / 'sprites.idasui').read_bytes(),
                                 menus.make_ui_pack(manifest['sprites'], payload))


if __name__ == '__main__':
    unittest.main()
