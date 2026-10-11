"""Recover night lamp coronas without re-exporting road, collision or scenery.

Usage: python Tools/Export-IdZeroLampGlows.py <IDZero package/data> RuntimeAssets
The PathV light positions are world-space fixture anchors. Gunsai's three
black, underground editor placeholders are not usable light sources.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path
from idas_efo import placements
from idas_yabx import Database


def lenses(pack, lights):
    """Keep the authored lens faces next to each actual fixture.

    Streetlight housings share an atlas/material with railings. Whole-material
    emission would light all that metal. The lens polygons use the small pale
    swatches at UV (0.316, 0.551) and (0.471, 0.399). Combine those swatches
    with proximity and face orientation, retaining tilted lens geometry.
    """
    raw = (pack / 'scene.bin').read_bytes()
    if raw[:4] != b'HKN2':
        raise ValueError('Scene header')
    at, faces, matched = 8, [], set()
    for _ in range(struct.unpack_from('<I', raw, 4)[0]):
        _, nv, nt = struct.unpack_from('<3I', raw, at)
        matrix = struct.unpack_from('<12f', raw, at+12)
        at += 60
        vertices = list(struct.iter_unpack('<10f4B', raw[at:at+nv*44]))
        at += nv*44
        triangles = struct.iter_unpack('<3I', raw[at:at+nt*4])
        at += nt*4
        # Lamp fixture shapes have identity transforms. Skip transformed skies.
        if matrix != (1.,0.,0.,0.,0.,1.,0.,0.,0.,0.,1.,0.):
            continue
        for tri in triangles:
            points = [vertices[i] for i in tri]
            if any(not ((.313 < v[6] < .320 and .548 < v[7] < .554) or
                        (.467 < v[6] < .475 and .393 < v[7] < .404)) for v in points):
                continue
            a = [points[1][k]-points[0][k] for k in range(3)]
            b = [points[2][k]-points[0][k] for k in range(3)]
            normal = (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])
            if abs(normal[1]) < math.sqrt(sum(x*x for x in normal))*.7:
                continue
            center = [sum(v[k] for v in points)/3 for k in range(3)]
            for i, light in enumerate(lights):
                if math.dist(center, light['position']) < .5 and all(math.dist(v[:3], light['position']) < .65 for v in points):
                    faces.append(dict(lamp=i, vertices=[x for v in points for x in v[:3]]))
                    matched.add(i)
                    break
    return faces, len(matched)


def export(source, packs):
    db = Database(source / 'EFFECT/LightGlare.efo')
    images = [o['file'] for o in db.objects if o.get('file', b'')[:4] == b'DDS ']
    if len(images) != 1:
        raise ValueError('Expected one LightGlare atlas')
    texture = images[0]
    report = {}
    for name in ('gunsai', 'odawara'):
        path = source / 'COURSE' / name / 'path' / (name + '_path_light.pa8')
        rows = placements(path)
        lights = []
        for row in rows:
            if not all(math.isfinite(x) for x in row):
                raise ValueError('Nonfinite lamp')
            if max(row[8:11]) <= 0:
                continue
            if not (0 < row[11] < 20 and all(0 <= x <= 1 for x in row[8:11])):
                raise ValueError('Unexpected lamp presentation values')
            lights.append(dict(position=row[:3], direction=row[4:7], color=row[8:11], size=row[11]))
        dest = packs / name.upper() / 'night_dry'
        # Full source atlas, sampled at its soft round corona tile by the shader.
        if lights:
            (dest / 'lamp-glare.dds').write_bytes(texture)
        manifest = dict(texture='lamp-glare.dds' if lights else '', lights=lights,
                        source='IDZero 2.20 '+name+' PathV / EFFECT/LightGlare.efo',
                        sourcePathSha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                        sourceTextureSha256=hashlib.sha256(texture).hexdigest())
        manifest['lenses'], matched = lenses(dest, lights) if lights else ([], 0)
        if matched != len(lights):
            raise ValueError(f'{name}: only {matched}/{len(lights)} fixture lenses recovered')
        (dest / 'lamp-glows.json').write_text(json.dumps(manifest, separators=(',', ':'))+'\n')
        report[name] = dict(sourceEntries=len(rows), visibleLights=len(lights), blackPlaceholders=len(rows)-len(lights), lensTriangles=len(manifest['lenses']), fixturesWithLenses=matched)
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('packs', type=Path)
    args = parser.parse_args()
    print(json.dumps(export(args.source, args.packs), indent=2))
