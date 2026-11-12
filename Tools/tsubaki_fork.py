"""Preserve the authored island inside Tsubaki's road ribbon.

The driving-limit paths enclose both branches at the fork. The source RCL2
marks the 26 triangles of its central island as blocked. Export that closed
boundary, then insert cross sections at every boundary corner so RCL1 contact
uses the same outline without changing the race paths or the handling model.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct

FIRST, LAST = 2964, 2980


def export_island(source, destination):
    data = source.read_bytes()
    header = struct.unpack_from('<16I', data)
    assert header[:2] == (0x52434c32, 2)
    assert header[5] + header[4] * 32 <= len(data)
    assert header[7] + header[6] * 32 <= len(data)
    vertices = [struct.unpack_from('<8f', data, header[5] + i * 32)
                for i in range(header[4])]
    triangles = [struct.unpack_from('<8i', data, header[7] + i * 32)
                 for i in range(header[6])]
    seed = 24296  # Surveyed fork island, not a roadside or an approach barrier.
    assert triangles[seed][6] & 0x80000000
    component, pending = {seed}, [seed]
    while pending:
        for neighbor in triangles[pending.pop()][3:6]:
            if neighbor >= 0 and neighbor not in component and triangles[neighbor][6] & 0x80000000:
                component.add(neighbor)
                pending.append(neighbor)
    assert component == set(range(24296, 24322)), 'Tsubaki source island changed; resurvey required'
    edges = [(t[(e + 2) % 3], t[e]) for i in sorted(component)
             for t in (triangles[i],) for e, neighbor in enumerate(t[3:6])
             if neighbor not in component]
    assert len(edges) == 28
    following = dict(edges)
    assert len(following) == len(edges) == len(set(following.values()))
    chain = [edges[0][0]]
    while following[chain[-1]] != chain[0]:
        chain.append(following[chain[-1]])
        assert len(chain) <= len(edges)
    assert len(chain) == len(edges)
    polygon = [vertices[i][:3] for i in chain]
    assert all(1099 < p[0] < 1116 and 365 < p[1] < 370 and 275 < p[2] < 288 for p in polygon)
    destination.write_text(json.dumps(dict(
        schema=1, source='Initial D Arcade Stage 8/data/COURSE/tsubaki/collision/tsubaki_collision.bin',
        sha256=hashlib.sha256(data).hexdigest(), sourceTriangles=sorted(component),
        sourceVertexIds=chain, polygon=polygon), indent=2) + '\n')


def partition_rows(root, roads):
    """Yield six columns per cross section; coincident columns share a vertex.

    Columns are outer-left, left, island-left, island-right, right, outer-right.
    The island columns collapse when absent. The resulting five strips are
    blocked / road / blocked / road / blocked. Existing rows outside this fork
    retain their four unique vertices and their original triangles.
    """
    manifest = json.loads((root / 'fork_island.json').read_text())
    assert manifest['schema'] == 1
    assert manifest['sourceTriangles'] == list(range(24296, 24322))
    polygon = manifest['polygon']
    assert len(polygon) == 28 and all(len(p) == 3 and all(math.isfinite(x) for x in p) for p in polygon)

    def sample(index, side):
        lo, fraction = int(index), index - int(index)
        if fraction == 0:
            return roads[side][lo]
        return tuple(a + fraction * (b - a) for a, b in zip(roads[side][lo], roads[side][lo + 1]))

    def signed(index, p):
        a, b = sample(index, 1), sample(index, 2)
        return (b[0] - a[0]) * (p[2] - a[2]) - (b[2] - a[2]) * (p[0] - a[0])

    indices = set(range(len(roads[0])))
    corner_indices = []
    for p in polygon:
        candidates = []
        for i in range(FIRST, LAST):
            start, end = signed(i, p), signed(i + 1, p)
            if start * end > 0:
                continue
            lo, hi = float(i), float(i + 1)
            for _ in range(44):
                mid = (lo + hi) / 2
                if signed(mid, p) * start > 0:
                    lo = mid
                else:
                    hi = mid
            index = (lo + hi) / 2
            a, b = sample(index, 1), sample(index, 2)
            dx, dz = b[0] - a[0], b[2] - a[2]
            fraction = ((p[0] - a[0]) * dx + (p[2] - a[2]) * dz) / (dx * dx + dz * dz)
            if 0 < fraction < 1 and abs(p[1] - sample(index, 0)[1]) < 2:
                candidates.append(index)
        assert len(candidates) == 1, ('Ambiguous fork corner', p, candidates)
        corner_indices.append(candidates[0])
        indices.add(candidates[0])
    first, last = min(corner_indices), max(corner_indices)
    for index in sorted(indices):
        left, right = sample(index, 1), sample(index, 2)
        dx, dz = right[0] - left[0], right[2] - left[2]
        full_length = math.dist(left, right)
        offset = tuple(8 * (b - a) / full_length for a, b in zip(left, right))
        inner = [left, left]
        if first <= index <= last:
            hits = []
            for a, b in zip(polygon, polygon[1:] + polygon[:1]):
                ex, ez = b[0] - a[0], b[2] - a[2]
                determinant = dx * ez - dz * ex
                if abs(determinant) < 1e-10:
                    continue
                ax, az = a[0] - left[0], a[2] - left[2]
                t = (ax * ez - az * ex) / determinant
                u = (ax * dz - az * dx) / determinant
                if -1e-8 <= u <= 1 + 1e-8 and 0 < t < 1:
                    hits.append(t)
            hits.sort()
            unique = []
            for t in hits:
                if not unique or t - unique[-1] > 1e-7:
                    unique.append(t)
            assert 1 <= len(unique) <= 2, ('Fork cross section changed', index, unique)
            if len(unique) == 1:
                assert abs(index - first) < 1e-7 or abs(index - last) < 1e-7
            inner = [tuple(a + t * (b - a) for a, b in zip(left, right))
                     for t in (unique[0], unique[-1])]
        yield [tuple(a - d for a, d in zip(left, offset)), left,
               *inner, right, tuple(b + d for b, d in zip(right, offset))]


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-collision', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    export_island(args.source_collision, args.output)
