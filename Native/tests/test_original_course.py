import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import extract_original_course as course

class OriginalCourseTests(unittest.TestCase):
    def test_all30_captured_original_draws_and_exported_matrices(self):
        base=ROOT/"data/original_models/courses/k_df"
        capture=json.loads((base/"source_capture.json").read_text())
        rows=course.validate_capture(capture)
        self.assertEqual(capture["total_original_instructions"],93967)
        for row in rows:
            with self.subTest(sector=row["sector"]):
                raw=(base/f"assembly/sector_{row['sector']:02d}.idasasm").read_bytes()
                self.assertEqual(raw[:8],b"IDAS3A1\0")
                self.assertEqual(struct.unpack_from("<2I",raw,8),(1,36))
                self.assertEqual(len(raw),16+68*36)
                decoded=[struct.unpack_from("<I16f",raw,16+i*68) for i in range(36)]
                expected=row["primary"]["chunks"]+row["static"]["chunks"]
                self.assertEqual([d[0] for d in decoded],expected)
                for d in decoded:self.assertEqual(list(d[1:]),course.identity())
                # Each road sector has exactly one near/far version, never both.
                for i in range(30):self.assertEqual(sum(c in expected for c in [i,30+i,60+i]),1)

    def test_capture_rejects_variant_overlap_or_missing_sector(self):
        base=ROOT/"data/original_models/courses/k_df"
        capture=json.loads((base/"source_capture.json").read_text())
        capture["sectors"][0]["primary"]["chunks"].append(60)
        with self.assertRaises(ValueError):course.validate_capture(capture)
        capture["sectors"].pop()
        with self.assertRaises(ValueError):course.validate_capture(capture)

    def test_concatenated_bank_provenance_and_boundaries(self):
        meta=json.loads((ROOT/"data/original_models/courses/k_df/scene_manifest.json").read_text())
        self.assertEqual([c["index"] for c in meta["chunks"]],list(range(135)))
        self.assertEqual([meta["source_banks"][b]["chunk_count"] for b in "abc"],[20,25,90])
        self.assertEqual(meta["boundaries"][0],105)
        self.assertEqual(meta["boundaries"][-1],4019)
        self.assertEqual(len(meta["boundaries"]),29)
        self.assertEqual(meta["texture_count"],211)
        for c in meta["chunks"]:
            self.assertGreater(c["vertex_count"],0)
            self.assertTrue(all(0<=t<211 for t in c["texture_indices"]))
            bank=meta["source_banks"][c["source_bank"]]
            self.assertEqual(c["index"],bank["first_global_chunk"]+c["source_local_index"])
            self.assertLessEqual(c["source_offset"]+c["source_size"],bank["files"]["polygon"]["bytes"])

if __name__=="__main__":unittest.main()
