import importlib.util
from pathlib import Path
import struct, unittest
spec=importlib.util.spec_from_file_location('texture_bank',Path(__file__).resolve().parents[1]/'tools/texture_bank.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class TextureBankTests(unittest.TestCase):
    def test_channel_roundtrip(self):
        for fmt in range(3):
            for value in range(65536):self.assertEqual(module.pack_pixel(module.expand(value,fmt),fmt),value)
    def test_morton_vectors(self):
        self.assertEqual([module.morton(x,y,2) for y in range(2) for x in range(2)],[0,2,1,3])
    def test_rectangular_permutation(self):
        for w,h in [(8,8),(8,64),(128,32),(16,32)]:
            indices=[module.texel_index(x,y,w,h,'twiddled') for y in range(h) for x in range(w)]
            self.assertEqual(sorted(indices),list(range(w*h)))
    def test_bounds_fail_closed(self):
        with self.assertRaises(ValueError):module.decode(bytes(3),8,8,0,'linear')
        with self.assertRaises(ValueError):module.decode(bytes(128),8,8,5,'linear')
        with self.assertRaises(ValueError):module.decode(bytes(120),6,10,0,'linear')
if __name__=='__main__':unittest.main()
