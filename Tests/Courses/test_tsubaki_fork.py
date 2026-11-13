"""Geometry checks for the authored island, independent of native swept tests."""
import json
import math
from pathlib import Path
import struct
import unittest

ROOT = Path(__file__).resolve().parents[2] / 'RuntimeAssets/TSUBAKI'


def area(points):
    return abs(sum(a[0] * b[2] - b[0] * a[2]
                   for a, b in zip(points, points[1:] + points[:1]))) / 2


def inside(p, polygon):
    odd = False
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        if (a[2] > p[2]) != (b[2] > p[2]):
            x = a[0] + (p[2] - a[2]) * (b[0] - a[0]) / (b[2] - a[2])
            if p[0] < x:
                odd = not odd
    return odd


def distance_to_boundary(p, polygon):
    distances = []
    for a, b in zip(polygon, polygon[1:] + polygon[:1]):
        dx, dz = b[0] - a[0], b[2] - a[2]
        fraction = max(0, min(1, ((p[0] - a[0]) * dx + (p[2] - a[2]) * dz) / (dx * dx + dz * dz)))
        distances.append(math.hypot(p[0] - a[0] - fraction * dx, p[2] - a[2] - fraction * dz))
    return min(distances)


class TsubakiFork(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = json.loads((ROOT / 'fork_island.json').read_text())
        cls.polygon = cls.source['polygon']
        raw = (ROOT / 'tsubaki.rcl').read_bytes()
        header = struct.unpack_from('<12I', raw)
        cls.vertices = [struct.unpack_from('<8f', raw, header[5] + i * 32)[:3]
                        for i in range(header[4])]
        cls.triangles = [struct.unpack_from('<8h', raw, header[7] + i * 16)
                         for i in range(header[6])]

    def test_source_island_provenance_and_area(self):
        self.assertEqual(self.source['sha256'], '83f7a18310ec392c9fcc9b751e1fb88798396625387c3a657979bb6dd16dcd93')
        self.assertEqual(self.source['sourceTriangles'], list(range(24296, 24322)))
        self.assertEqual(len(self.polygon), 28)
        self.assertAlmostEqual(area(self.polygon), 31.31657109037, places=5)

    def test_no_drivable_triangle_crosses_island_and_blocked_area_is_preserved(self):
        blocked = 0
        for t in self.triangles:
            points = [self.vertices[i] for i in t[:3]]
            p = tuple(sum(q[k] for q in points) / 3 for k in range(3))
            if inside(p, self.polygon):
                self.assertLess(t[7], 0, 'Island contains a drivable triangle')
                blocked += area(points)
        self.assertAlmostEqual(blocked, area(self.polygon), delta=.003)

    def test_closed_island_has_reciprocal_contacts_on_authored_boundary(self):
        component = set()
        for i, t in enumerate(self.triangles):
            p = tuple(sum(self.vertices[j][k] for j in t[:3]) / 3 for k in range(3))
            if inside(p, self.polygon):
                component.add(i)
        self.assertGreater(len(component), 26)
        boundary_vertices = set()
        length = 0
        for i in component:
            t = self.triangles[i]
            for e, neighbor in enumerate(t[3:6]):
                self.assertGreaterEqual(neighbor, 0, 'Island has an unconnected triangle')
                self.assertIn(i, self.triangles[neighbor][3:6])
                if neighbor in component:
                    continue
                self.assertGreaterEqual(self.triangles[neighbor][7], 0)
                a, b = (self.vertices[t[k]] for k in ((e + 2) % 3, e))
                for p in (a, b):
                    self.assertLess(distance_to_boundary(p, self.polygon), .0002)
                    boundary_vertices.add((p[0], p[2]))
                length += math.hypot(a[0] - b[0], a[2] - b[2])
        expected = sum(math.hypot(a[0] - b[0], a[2] - b[2])
                       for a, b in zip(self.polygon, self.polygon[1:] + self.polygon[:1]))
        self.assertAlmostEqual(length, expected, delta=.003)
        for p in self.polygon:
            self.assertLess(min(math.hypot(p[0] - x, p[2] - z) for x, z in boundary_vertices), .0002)


if __name__ == '__main__':
    unittest.main()
