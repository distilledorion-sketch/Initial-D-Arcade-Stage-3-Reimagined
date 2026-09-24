#!/usr/bin/env python3
"""Import actual recovered hanging ornaments, never inventory thumbnails.

Requires the read-only glTF recovery produced for The Arcade S3. The small ORN1
format stores original mesh triangles with a documented coordinate conversion.
Diffuse PNG files are copied byte for byte. Assembly and car-responsive motion
are implemented by the game, not claimed to be recovered native behavior.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import struct
import uuid

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE = Path('D:/Initial D games/Extracted Ornaments - The Arcade S3')
NAMESPACE = uuid.UUID('a4544d5d-ed50-4e57-8497-54e1a743dd5a')
COMPONENTS = {'SCALAR': 1, 'VEC2': 2, 'VEC3': 3, 'VEC4': 4, 'MAT4': 16}
FORMATS = {5121: 'B', 5123: 'H', 5125: 'I', 5126: 'f'}


def read_json(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, separators=(',', ':'), allow_nan=False) + '\n', encoding='utf-8')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def vector(value):
    return dict(zip(('x', 'y', 'z'), value))


def color(value):
    return dict(zip(('r', 'g', 'b', 'a'), value))


def unity(value):
    # glTF export +Z hangs down, decorated +Y faces the viewer.
    return value[0], -value[2], -value[1]


def bind_position(matrix):
    """Invert the affine inverse-bind matrix (glTF column-major storage)."""
    a, b, c, d, e, f, g, h, i = (matrix[n] for n in (0, 4, 8, 1, 5, 9, 2, 6, 10))
    det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g)
    if abs(det) < 1e-10:
        raise ValueError('Singular chain inverse bind matrix')
    inverse = ((e*i-f*h, c*h-b*i, b*f-c*e), (f*g-d*i, a*i-c*g, c*d-a*f), (d*h-e*g, b*g-a*h, a*e-b*d))
    return tuple(-sum(row[k] * matrix[12+k] for k in range(3)) / det for row in inverse)


def display_name(row):
    item_id = row['id']
    mesh = row['mesh']['package'].split('/')[-1]
    if item_id < 200:
        return row['name_nfkc'].replace('トレノ', ' Trueno').replace('レビン', ' Levin').replace('2ドア', ' 2-door').replace('エボ', ' Evo ')
    courses = ['Akina Lake', 'Myogi', 'Akagi', 'Akina', 'Irohazaka', 'Tsukuba', 'Happogahara', 'Nagao', 'Tsubaki Line', 'Usui', 'Sadamine', 'Tsuchisaka', 'Akina Snow', 'Hakone', 'Momiji Line', 'Nanamagari', 'Gunsai', 'Odawara', 'Tsukuba Snow', 'Tsuchisaka Snow', 'Yabitsu']
    for first, suffix in [(200, 'Wood Tag'), (225, 'Red Plate'), (250, 'Black Plate')]:
        if first <= item_id <= first + 20:
            return courses[item_id - first] + ' ' + suffix
    if 300 <= item_id <= 336:
        return 'Initial D Quote %02d' % (item_id - 299)
    names = {
        530: 'Japan Road Trip Strap', 531: 'World Tour Strap',
        535: 'Victory Power Stone', 536: 'Lucky Power Stone', 537: 'Speed Power Stone',
        538: 'Cassette Tape Black', 539: 'Cassette Tape Red', 540: 'Cassette Tape Blue',
        541: 'Turbo Gold', 542: 'Turbo Silver', 543: 'Turbo Bronze',
        544: 'Rotary Gold', 545: 'Rotary Silver', 546: 'Rotary Bronze',
        557: 'Hot Spring Bun', 558: 'Christmas Cake', 559: 'Katsudon', 560: 'Steampunk Keychain',
        700: 'Wangan Midnight Silver', 701: 'Wangan Midnight Blue', 702: 'Devil Z Number Plate',
        703: 'Blackbird Number Plate', 704: 'Kitami Cycle', 705: 'B Wangan Route Sign', 706: 'C1 Route Sign',
        707: 'Lifeguard Logo', 708: 'Lifeguard Can', 709: 'Lifeguard Infinity', 710: 'Lifeguard Bottle Cap',
        711: 'Lifeguard Girl', 712: 'Usadar', 713: 'Reimu Hakurei', 714: 'Marisa Kirisame', 715: 'Aya Shameimaru',
        716: 'Reimu Player Sprite', 717: 'Marisa Player Sprite', 718: 'Aya Player Sprite',
        719: 'Touhou Power Item', 720: 'Touhou Point Item', 721: 'Touhou Bomb Item', 722: 'Touhou 1up Item',
        723: 'Kanata Katagiri', 724: 'Shun Aiba', 725: 'Michael Beckenbauer', 726: 'Sena Moroboshi',
        727: 'MF Ghost Logo', 728: 'MF Ghost Drone', 729: 'Ren Comic Cover', 730: 'CHUNITHM NEW Logo',
        731: 'CHUNITHM Penguin', 732: 'Hot-Version Logo', 733: 'Maou',
        1000: 'Takumi Fujiwara', 1001: 'Keisuke Takahashi', 1002: 'Koichiro Iketani', 1003: 'Sayuki',
        1004: 'Wataru Akiyama', 1005: 'Kyoichi Sudo', 1006: 'Seiji Iwaki', 1007: 'Takeshi Nakazato',
        1008: 'Ryosuke Takahashi', 1009: 'Mako Sato', 1010: 'Shingo Shoji', 1011: 'Kai Kogashiwa',
        1012: 'Daiki Ninomiya', 1013: 'Smiley Sakai', 1014: 'Bunta Fujiwara', 1015: 'Natsuki Mogi',
        1100: 'Tofu Delivery Case', 1101: 'Fireball', 1102: 'Fried Tofu', 1103: '47 kg Weight',
        1104: 'Adrenaline Drink', 1105: 'Bandana', 1106: 'Irohazaka Monkey', 1107: 'Bodywork Hammer',
        1108: 'Rose Bouquet', 1109: 'Sandals', 1110: 'Duct Tape', 1111: 'Motorcycle',
        1112: 'Brake Disc', 1113: 'Sudden Braking Plate', 1114: 'Lighter', 1115: 'Motion Sickness Medicine',
        1200: 'Message Button', 1201: 'Smartphone',
    }
    if item_id in names:
        return names[item_id]
    if 547 <= item_id <= 556:
        return ['Initial D Logo', 'Initial D Ver.2 Logo', 'Initial D Ver.3 Logo', 'Initial D 4 Logo', 'Initial D 5 Logo', 'Initial D 6 Logo', 'Initial D 7 Logo', 'Initial D 8 Logo', 'Initial D Zero Logo', 'Initial D The Arcade Logo'][item_id - 547]
    result = re.sub('^key_chain_', '', mesh, flags=re.I).replace('_', ' ')
    for source, target in [('Husa', 'Tassel'), ('Brack', 'Black'), ('Wheel', 'Tire'), ('Momiji', 'Maple Leaf'), ('Amulet', 'Lucky Charm'), ('Lightgreen', 'Green'), ('Lightblue', 'Aqua')]:
        result = result.replace(source, target)
    return result


class Importer:
    def __init__(self, source, output):
        self.source, self.output = source, output
        self.models = source / 'Models'
        self.textures = {}
        self.records = []
        self.texture_template = (ROOT / 'Assets/Resources/ArcadeHud/Meter31/T_Meter31_LampEf_03.png.meta').read_text(encoding='utf-8')

    def meta(self, path, texture=False):
        meta = Path(str(path) + '.meta')
        if meta.exists():
            return
        guid = uuid.uuid5(NAMESPACE, path.relative_to(ROOT).as_posix()).hex
        if texture:
            text = re.sub(r'(?m)^guid: .*', 'guid: ' + guid, self.texture_template)
            text = text.replace('maxTextureSize: 1024', 'maxTextureSize: 2048')
            text = text.replace('wrapU: 1', 'wrapU: 0').replace('wrapV: 1', 'wrapV: 0')
        else:
            importer = 'DefaultImporter' if path.is_dir() else 'TextScriptImporter'
            text = 'fileFormatVersion: 2\nguid: ' + guid + '\n' + ('folderAsset: yes\n' if path.is_dir() else '') + importer + ':\n  externalObjects: {}\n  userData: \n  assetBundleName: \n  assetBundleVariant: \n'
        meta.write_text(text, encoding='utf-8')

    def texture(self, path):
        path = path.resolve()
        if path in self.textures:
            return self.textures[path]
        if not path.is_relative_to(self.models.resolve()) or path.suffix.lower() != '.png':
            raise ValueError('Texture outside verified model recovery: ' + str(path))
        relative = path.relative_to(self.models.resolve())
        if 'icon' in str(relative).lower():
            raise ValueError('UI icon cannot be imported as ornament artwork: ' + str(path))
        target = self.output / 'Textures' / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        source_hash = digest(path)
        if not target.exists() or digest(target) != source_hash:
            shutil.copyfile(path, target)
        assert digest(target) == source_hash
        self.meta(target, texture=True)
        resource = target.relative_to(ROOT / 'Assets/Resources').with_suffix('').as_posix()
        self.textures[path] = resource
        self.records.append({'source': str(path), 'output': target.relative_to(ROOT).as_posix(), 'sha256': source_hash, 'bytes': target.stat().st_size, 'byteIdentical': True})
        return resource

    def chain_rig(self, relative, strap_id):
        """Preserve the recovered skin palette and weights in ORN1 vertex order."""
        path = self.models / (relative + '.gltf')
        model = read_json(path)
        buffers = [(path.parent / b['uri']).read_bytes() for b in model['buffers']]

        def accessor(index):
            item = model['accessors'][index]
            view = model['bufferViews'][item['bufferView']]
            fmt = '<' + FORMATS[item['componentType']] * COMPONENTS[item['type']]
            offset = item.get('byteOffset', 0) + view.get('byteOffset', 0)
            stride = view.get('byteStride', struct.calcsize(fmt))
            values = [struct.unpack_from(fmt, buffers[view['buffer']], offset + n * stride) for n in range(item['count'])]
            if item.get('normalized'):
                divisor = {5121: 255, 5123: 65535}[item['componentType']]
                values = [tuple(v / divisor for v in row) for row in values]
            return values

        if len(model.get('skins', [])) != 1:
            raise ValueError('Expected one recovered chain skin: ' + str(path))
        skin = model['skins'][0]
        names = [model['nodes'][index]['name'] for index in skin['joints']]
        joints = [vector(unity(bind_position(m))) for m in accessor(skin['inverseBindMatrices'])]
        assert len(joints) == len(names) and 'ChainJoint0' in names
        parts = []
        for mesh in model['meshes']:
            for primitive in mesh['primitives']:
                attributes = primitive['attributes']
                indices, weights = accessor(attributes['JOINTS_0']), accessor(attributes['WEIGHTS_0'])
                assert len(indices) == len(weights) == model['accessors'][attributes['POSITION']]['count']
                recovered_weights = []
                for palette, influences in zip(indices, weights):
                    assert len(palette) == len(influences) == 4
                    assert all(isinstance(j, int) and 0 <= j < len(joints) for j in palette)
                    assert all(math.isfinite(w) and 0 <= w <= 1 for w in influences)
                    total = sum(influences)
                    if not .999 <= total <= 1.001:
                        raise ValueError('Invalid recovered chain skin weight sum')
                    recovered_weights.append({'joints': list(palette), 'weights': [w / total for w in influences]})
                parts.append({'weights': recovered_weights})
        target = self.output / 'Rigs' / ('strap_%04d.json' % strap_id)
        value = {'version': 1, 'strapId': strap_id, 'fixedJoint': names.index('ChainJoint0'),
            'jointNames': names, 'joints': joints, 'parts': parts}
        write_json(target, value)
        self.meta(target)
        self.meta(target.parent)
        record = {'strapId': strap_id, 'source': str(path), 'sourceSha256': digest(path),
            'geometrySha256': [hashlib.sha256(b).hexdigest() for b in buffers],
            'output': target.relative_to(ROOT).as_posix(), 'sha256': digest(target), 'bytes': target.stat().st_size,
            'joints': len(joints), 'partVertices': [len(p['weights']) for p in parts],
            'coordinateSystem': 'Recovered inverse-bind positions converted by (x,-z,-y); palette and ORN1 primitive/vertex order retained'}
        self.records.append(record)
        return record

    def rigs_only(self):
        source_catalog = read_json(self.source / 'Catalog/ornaments.json')
        records = [self.chain_rig(row['mesh']['package'].removeprefix('GameProject/Content/'), row['id'])
            for row in source_catalog['straps']]
        assert len(records) == 4
        report = {'straps': 4, 'recoveredSkinWeights': True, 'files': records}
        write_json(ROOT / 'Verification/ornaments-20260924/chain-rig-import-manifest.json', report)
        print(json.dumps({k: v for k, v in report.items() if k != 'files'}))

    def model(self, relative, key):
        path = self.models / (relative + '.gltf')
        model = read_json(path)
        buffers = [(path.parent / b['uri']).read_bytes() for b in model['buffers']]
        for node in model['nodes']:
            if 'mesh' in node and any(k in node for k in ['matrix', 'rotation', 'translation', 'scale']):
                raise ValueError('Unexpected transformed mesh node: ' + str(path))
        def accessor(index):
            item = model['accessors'][index]
            view = model['bufferViews'][item['bufferView']]
            fmt = '<' + FORMATS[item['componentType']] * COMPONENTS[item['type']]
            size = struct.calcsize(fmt)
            offset = item.get('byteOffset', 0) + view.get('byteOffset', 0)
            stride = view.get('byteStride', size)
            return [struct.unpack_from(fmt, buffers[view['buffer']], offset + n * stride) for n in range(item['count'])]
        materials = []
        for material in model['materials']:
            pbr = material['pbrMetallicRoughness']
            texture_index = pbr['baseColorTexture']['index']
            image_index = model['textures'][texture_index]['source']
            texture_path = path.parent / model['images'][image_index]['uri']
            materials.append({'name': material['name'], 'texture': self.texture(texture_path),
                'alphaCutoff': material.get('alphaCutoff', .333) if material.get('alphaMode') == 'MASK' else 0,
                'transparent': material.get('alphaMode') == 'BLEND', 'doubleSided': material.get('doubleSided', False),
                'tint': color(pbr.get('baseColorFactor', [1, 1, 1, 1]))})
        primitives = [p for m in model['meshes'] for p in m['primitives']]
        binary = bytearray(b'ORN1' + struct.pack('<i', len(primitives)))
        bounds_min, bounds_max = [math.inf] * 3, [-math.inf] * 3
        vertices = triangles = 0
        for primitive in primitives:
            assert primitive.get('mode', 4) == 4
            attributes = primitive['attributes']
            positions = accessor(attributes['POSITION'])
            normals = accessor(attributes['NORMAL'])
            uv = accessor(attributes['TEXCOORD_0'])
            indices = [x[0] for x in accessor(primitive['indices'])]
            assert len(positions) == len(normals) == len(uv) and len(indices) % 3 == 0
            assert all(0 <= n < len(positions) for n in indices)
            binary.extend(struct.pack('<iii', primitive['material'], len(positions), len(indices)))
            for position, normal, texcoord in zip(positions, normals, uv):
                position, normal = unity(position), unity(normal)
                packed = (*position, *normal, texcoord[0], 1 - texcoord[1])
                assert all(math.isfinite(v) for v in packed)
                binary.extend(struct.pack('<8f', *packed))
                for axis in range(3):
                    bounds_min[axis] = min(bounds_min[axis], position[axis])
                    bounds_max[axis] = max(bounds_max[axis], position[axis])
            # Reflecting the glTF coordinate handedness requires winding reversal.
            for offset in range(0, len(indices), 3):
                binary.extend(struct.pack('<iii', indices[offset], indices[offset + 2], indices[offset + 1]))
            vertices += len(positions)
            triangles += len(indices) // 3
        target = self.output / 'Meshes' / (key + '.bytes')
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(binary)
        self.meta(target)
        self.records.append({'source': str(path), 'sourceSha256': digest(path), 'geometrySha256': [hashlib.sha256(b).hexdigest() for b in buffers],
            'output': target.relative_to(ROOT).as_posix(), 'sha256': digest(target), 'vertices': vertices, 'triangles': triangles, 'bytes': len(binary)})
        return {'mesh': target.relative_to(ROOT / 'Assets/Resources').with_suffix('').as_posix(), 'sourceModel': relative,
            'boundsMin': vector(bounds_min), 'boundsMax': vector(bounds_max), 'materials': materials, 'vertices': vertices, 'triangles': triangles}

    def run(self):
        source_catalog = read_json(self.source / 'Catalog/ornaments.json')
        sockets = {r['id']: r['sockets'] for r in read_json(self.source / 'Catalog/ornament-socket-notes.json')['ornaments']}
        ornaments, straps = [], []
        for row in source_catalog['ornaments']:
            relative = row['mesh']['package'].removeprefix('GameProject/Content/')
            entry = self.model(relative, 'ornament_%04d' % row['id'])
            entry.update(id=row['id'], name=display_name(row), sourceName=row['name_nfkc'], strapId=row['strap_id'], swingType=row['swing_type'],
                recoveredSocket=bool(sockets[row['id']]), socketScale=.5 if sockets[row['id']] else 1,
                attachmentInferred=True)
            ornaments.append(entry)
        for row in source_catalog['straps']:
            relative = row['mesh']['package'].removeprefix('GameProject/Content/')
            entry = self.model(relative, 'strap_%02d' % row['id'])
            entry.update(id=row['id'], name='Chain %d' % row['id'], swingType=row['swing_type'], bindPose=True)
            straps.append(entry)
            self.chain_rig(relative, row['id'])
        assert len(ornaments) == 280 and len(straps) == 4
        assert len({r['id'] for r in ornaments}) == len(ornaments)
        catalog = {'version': 1, 'coordinateSystem': 'Unity local metres; origin retained; +X right; -Y hangs down; decorated face -Z; UV V flipped',
            'ornaments': ornaments, 'straps': straps,
            'limitations': ['Original native strap assembly transform was not recovered. Mounting and car-responsive pendulum motion are reconstructed.',
                'Skeletal chains retain recovered bind poses and skin weights. Runtime chain motion is reconstructed.',
                'Original diffuse textures, masked alpha and sidedness are retained. Cooked Unreal material graphs are not executable in Unity.']}
        target = self.output / 'catalog.json'
        write_json(target, catalog)
        self.meta(target)
        for folder in [self.output, *sorted((p for p in self.output.rglob('*') if p.is_dir()), key=str)]:
            self.meta(folder)
        report = {'ornaments': len(ornaments), 'straps': len(straps), 'meshes': len(ornaments) + len(straps), 'textures': len(self.textures),
            'sourceSockets': sum(r['recoveredSocket'] for r in ornaments), 'vertices': sum(r['vertices'] for r in ornaments + straps),
            'triangles': sum(r['triangles'] for r in ornaments + straps), 'meshBytes': sum(r['bytes'] for r in self.records if 'triangles' in r),
            'textureBytes': sum(r['bytes'] for r in self.records if r.get('byteIdentical')), 'files': self.records, 'limitations': catalog['limitations']}
        write_json(ROOT / 'Verification/ornaments-20260924/import-manifest.json', report)
        print(json.dumps({k: v for k, v in report.items() if k not in ('files', 'limitations')}))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=DEFAULT_SOURCE)
    parser.add_argument('--output', type=Path, default=ROOT / 'Assets/Resources/ArcadeOrnaments')
    parser.add_argument('--rigs-only', action='store_true', help='Import the four recovered chain rigs without rewriting geometry or textures.')
    args = parser.parse_args()
    importer = Importer(args.source, args.output)
    importer.rigs_only() if args.rigs_only else importer.run()
