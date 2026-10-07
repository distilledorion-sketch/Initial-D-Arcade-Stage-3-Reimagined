"""Export YABX scenery into the shared HKN2 imported-course renderer format."""
import hashlib
import json
import re
import struct
from pathlib import Path
from idas_yabx import Database, string


IDENTITY = (1., 0., 0., 0., 0., 1., 0., 0., 0., 0., 1., 0.)


def number(obj, key):
    return int.from_bytes(obj[key], 'little')


def floats(raw):
    return struct.unpack('<' + 'f' * (len(raw) // 4), raw)


def placements(path):
    raw = Path(path).read_bytes()
    assert raw[:5] == b'PathV' and len(raw) == 32 + number({'n': raw[12:16]}, 'n') * 48
    return list(struct.iter_unpack('<12f', raw[32:]))


class Scene:
    def __init__(self, output, course='Gunsai'):
        self.course = course
        self.output = Path(output)
        self.output.mkdir(parents=True, exist_ok=True)
        self.materials, self.shapes, self.instances = [], [], []
        self.triangles = 0

    def archive(self, path, kind='course'):
        db = Database(path)
        materials, headers = {}, {}
        transforms = {}
        for obj in db.objects:
            if obj['_class'].endswith('::_sBone'):
                header = db.ref(obj['shapeHeader'])
                # Gunsai's authored bones are identity, including tree roots.
                # Refuse a different hierarchy rather than silently flatten it.
                # Odawara turns and stretches its sky dome; the row-major 3x4
                # matrix is written with the shape and applied by the loader.
                matrix = floats(obj['mtxLocal'])
                assert matrix == IDENTITY or kind == 'sky' and not db.ref(obj['parent']), (
                    path, string(obj['name']), matrix)
                if header:
                    transforms[header['_index']] = matrix
        for header in db.objects:
            if not header['_class'].endswith('::_sShapeHeader'):
                continue
            indices = headers.setdefault(string(header['name']), [])
            for shape in db.refs(header['shape']):
                state = db.ref(shape['state'])
                if state['_index'] not in materials:
                    textures = []
                    for texture in db.refs(state['texture']):
                        texture_type = number(texture, 'textureType')
                        if texture_type not in (1, 6):
                            continue  # D3 renderer has no IDZero normal/specular pass.
                        assert floats(texture['uvScale']) == (1., 1.) and floats(texture['uvOffset']) == (0., 0.)
                        image = db.ref(texture['textureImage'])
                        raw = image['file']
                        assert raw[:4] == b'DDS '
                        filename = hashlib.sha256(raw).hexdigest()[:24] + '.dds'
                        destination = self.output / filename
                        if not destination.exists():
                            destination.write_bytes(raw)
                        textures.append(dict(file=filename, uv=number(texture, 'uvSetIndex'), type=texture_type))
                    material = dict(name=string(state['name']), kind=kind, textures=textures,
                                    cutoff=number(state, 'alphaRef')/255,
                                    shadow=len(textures) == 1 and textures[0]['type'] == 6,
                                    sky=kind == 'sky')
                    materials[state['_index']] = len(self.materials)
                    self.materials.append(material)
                display = db.ref(shape['displayList'])
                geometry = db.ref(display['geometry'])
                array = db.ref(geometry['vertexArray'])
                layout = array['_class'].split('VertexArray')[-1]
                stride = number(geometry, 'strideSize')
                layouts = {'PT': (20, None, None, 12), 'PCT': (24, None, 12, 16),
                           'PNT': (32, 12, None, 24), 'PNCT': (36, 12, 24, 28),
                           'PBCT': (52, 12, 40, 44), 'PBCT2': (60, 12, 40, 44),
                           'PN': (24, 12, None, None), 'PNC': (28, 12, 24, None),
                           'PNCT2': (44, 12, 24, 28)}
                expected, normal_at, color_at, uv_at = layouts[layout]
                assert stride == expected
                raw_vertices = array['array']
                vertex_count = struct.unpack_from('<I', raw_vertices)[0]
                assert len(raw_vertices) == 4 + vertex_count * stride
                owner = display
                seen = set()
                while len(owner['index']) == 4:
                    assert owner['_index'] not in seen
                    seen.add(owner['_index'])
                    owner = db.ref(owner['displayListRef'])
                    assert owner is not None
                raw_indices = owner['index']
                count = struct.unpack_from('<I', raw_indices)[0]
                assert len(raw_indices) == 4 + count*2
                all_indices = struct.unpack_from('<' + 'H'*count, raw_indices, 4)
                triangles = []
                for primitive in db.refs(display['primitiveList']):
                    start, count = number(primitive, 'indexStart'), number(primitive, 'indexNumber')
                    strip = all_indices[start:start+count]
                    assert len(strip) == count and number(primitive, 'primitiveType') == 4
                    for i in range(2, count):
                        tri = (strip[i-2], strip[i-1], strip[i]) if i % 2 == 0 else (strip[i-1], strip[i-2], strip[i])
                        if len(set(tri)) == 3:
                            triangles.extend(tri)
                remap = dict.fromkeys(triangles)
                vertices = bytearray()
                for new, old in enumerate(remap):
                    assert old < vertex_count
                    remap[old] = new
                    at = 4 + old*stride
                    pos = struct.unpack_from('<3f', raw_vertices, at)
                    normal = struct.unpack_from('<3f', raw_vertices, at+normal_at) if normal_at else (0., 1., 0.)
                    uv = struct.unpack_from('<2f', raw_vertices, at+uv_at) if uv_at else (0., 0.)
                    uv2 = struct.unpack_from('<2f', raw_vertices, at+uv_at+8) if layout.endswith('2') else uv
                    color = raw_vertices[at+color_at:at+color_at+4] if color_at else b'\xff'*4
                    vertices += struct.pack('<10f', *pos, *normal, *uv, *uv2) + color
                index_bytes = struct.pack('<'+'I'*len(triangles), *(remap[i] for i in triangles))
                indices.append(len(self.shapes))
                self.shapes.append(struct.pack('<3I12f', materials[state['_index']], len(remap), len(triangles),
                                               *transforms.get(header['_index'], IDENTITY)) + vertices + index_bytes)
                self.triangles += len(triangles)//3
        print(f'{Path(path).name}: {len(headers)} headers, {sum(map(len, headers.values()))} meshes', flush=True)
        return headers

    def trees(self, path, headers):
        models = {}
        for name, meshes in headers.items():
            match = re.match(r'([abc])(\d\d)', name)
            assert match and len(meshes) == 1
            models[match[1], int(match[2])] = meshes[0]
        for row in placements(path):
            model = round(row[3])
            lods = [models.get((level, model), models['b', model]) for level in 'abc']
            self.instances.append(dict(kind='tree', meshes=lods, position=row[:3], forward=row[4:7],
                                       angles=row[8:11], scale=row[11]))

    def save(self, lighting_path, path_points):
        db = Database(lighting_path)
        blender = db.objects[0]
        lighting = [dict(name=string(o['sName']), sunDirection=floats(o['v3CourseSunDirection']),
                         fogColor=floats(o['v3CarFogColor']), distanceScale=floats(o['fCourseDistanceScale'])[0])
                    for o in db.refs(blender['_vpConfig'])]
        events = []
        for obj in db.refs(blender['_vpEvent']):
            raw = obj['_vEvent']; count = struct.unpack_from('<I', raw)[0]
            assert len(raw) == 4 + count*8
            events.append(dict(points=[dict(point=p, profile=i) for p, i in struct.iter_unpack('<fI', raw[4:])]))
        assert len(events) == 2
        manifest = dict(materials=self.materials, instances=self.instances, lighting=lighting, lightingEvents=events,
                        pathPoints=path_points, shapes=len(self.shapes), triangles=self.triangles,
                        source=f'Initial D Arcade Stage Zero 2.20 / {self.course} (YABX authored scenery)')
        (self.output/'scene.bin').write_bytes(b'HKN2'+struct.pack('<I', len(self.shapes))+b''.join(self.shapes))
        (self.output/'scene.json').write_text(json.dumps(manifest, separators=(',', ':'))+'\n')
