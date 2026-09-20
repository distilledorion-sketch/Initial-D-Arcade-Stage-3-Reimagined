import importlib.util
import struct
import tempfile
import unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location("extract_original_models",ROOT/"tools/extract_original_models.py")
extract=importlib.util.module_from_spec(spec)
spec.loader.exec_module(extract)

class OriginalModelTests(unittest.TestCase):
    def test_independent_strip_and_fan_boundaries(self):
        vertices=[(0,), (0,), (0x60000000,), (0xA0000000,),
                  (0,), (0,), (0x40000000,), (0xC0000000,)]
        self.assertEqual(extract.triangles(vertices),[0,1,2,2,1,3,4,5,6,4,6,7])

    def test_packed_normal_sign(self):
        self.assertEqual(extract.packed_normal(0x00ff007f),(1.0,0.0,-1.0/127.0))

    def test_nmzip_suffix_is_not_compression(self):
        with tempfile.TemporaryDirectory() as directory:
            p=Path(directory)/"model.bin.nz"
            p.write_bytes(b"\x00\x01\x00\x00original")
            self.assertEqual(extract.read_payload(p),p.read_bytes())

    def test_actual_export_raw_vertex_bytes(self):
        directory=ROOT/"data/original_models/toyota_ae86t"
        p=directory/"toyota_ae86t.idasmesh"
        if not p.exists(): self.skipTest("Private original AE86 export unavailable")
        b=p.read_bytes()
        self.assertEqual(b[:8],b"IDAS3M1\0")
        version,chunks=struct.unpack_from("<II",b,8)
        self.assertEqual((version,chunks),(1,106))
        cursor,batches,vertices,triangles=16,0,0,0
        for expected_chunk in range(chunks):
            chunk,source,size,count=struct.unpack_from("<4I",b,cursor);cursor+=16
            header=struct.unpack_from("<24I",b,cursor);cursor+=96
            self.assertEqual(chunk,expected_chunk)
            self.assertEqual(header[0],0x100)
            self.assertEqual(header[6],size)
            for _ in range(count):
                offset,nv,ni,raw_bytes=struct.unpack_from("<4I",b,cursor);cursor+=16
                ich=struct.unpack_from("<8I",b,cursor);cursor+=32
                material=struct.unpack_from("<16I",b,cursor);cursor+=64
                self.assertGreaterEqual(offset,source+96)
                self.assertEqual(nv,ich[7])
                stride=extract.VERTEX_SIZE[ich[6]]
                self.assertEqual(raw_bytes,nv*stride)
                normalized_start=cursor;cursor+=nv*44
                indices=struct.unpack_from(f"<{ni}I",b,cursor);cursor+=ni*4
                raw_start=cursor;cursor+=raw_bytes
                self.assertEqual(ni%3,0)
                self.assertTrue(all(i<nv for i in indices))
                for i in range(nv):
                    # Header and XYZ are byte-for-byte identical to source.
                    self.assertEqual(b[normalized_start+i*44:normalized_start+i*44+16],b[raw_start+i*stride:raw_start+i*stride+16])
                    if ich[6]==0xA:
                        self.assertEqual(b[normalized_start+i*44+28:normalized_start+i*44+36],b[raw_start+i*stride+16:raw_start+i*stride+24])
                batches+=1;vertices+=nv;triangles+=ni//3
        self.assertEqual(cursor,len(b))
        self.assertEqual((batches,vertices,triangles),(349,39566,29476))

if __name__=="__main__":unittest.main()
