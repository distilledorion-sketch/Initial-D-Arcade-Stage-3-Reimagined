import importlib.util
import json
import math
from pathlib import Path
import struct
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("assembly_export",ROOT/"tools/export_original_assembly.py")
assembly=importlib.util.module_from_spec(spec)
spec.loader.exec_module(assembly)

class OriginalAssemblyTests(unittest.TestCase):
    def test_column_vector_order_and_axes(self):
        ops=[{"type":"translate","values":[2,3,4]},
             {"type":"rotate_y_u16","values":[16384]},
             {"type":"scale","values":[2,2,2]}]
        m=assembly.operations_matrix(ops)
        # Postmultiplication: point(0,0,1) is scaled, then turned to+X,
        # then translated; the original matrix layout must not transpose this.
        actual=[m[r*4+2]+m[r*4+3] for r in range(3)]
        for a,b in zip(actual,[4,3,4]):self.assertAlmostEqual(a,b,places=6)

    def test_uncalibrated_float_angle_rejected(self):
        with self.assertRaises(ValueError):
            assembly.operations_matrix([{"type":"rotate_x_float","values":[1]}])

    def test_all35_native_assemblies_are_bounded_affine(self):
        catalog=json.loads((ROOT/"data/original_models/car_catalog.json").read_text())
        self.assertEqual([r["car_index"] for r in catalog["cars"]],list(range(35)))
        count=0
        for row in catalog["cars"]:
            with self.subTest(car=row["car_index"]):
                path=ROOT/row["assembly"]
                data=path.read_bytes()
                self.assertEqual(data[:8],b"IDAS3A1\0")
                version,n=struct.unpack_from("<2I",data,8)
                self.assertEqual(version,1)
                self.assertGreater(n,0)
                self.assertEqual(len(data),16+n*68)
                meta=json.loads((path.parent/"assembly_manifest.json").read_text())
                bank=json.loads((path.parent.parent/"model_manifest.json").read_text())
                self.assertEqual(meta["capture"]["final_matrix_depth"],0)
                for i in range(n):
                    values=struct.unpack_from("<I16f",data,16+i*68)
                    chunk,m=values[0],values[1:]
                    self.assertLess(chunk,bank["chunk_count"])
                    self.assertGreater(bank["chunks"][chunk]["vertex_count"],0)
                    self.assertTrue(all(math.isfinite(v) for v in m))
                    self.assertEqual(m[12:],(0,0,0,1))
                count+=n
        self.assertEqual(count,659)

if __name__=="__main__":unittest.main()
