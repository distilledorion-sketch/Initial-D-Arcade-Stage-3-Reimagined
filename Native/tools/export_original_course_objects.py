#!/usr/bin/env python3
"""Export original 083FC0 tree owners into additive native scene sidecars.

Requires independently captured constructor settings and primary/static owner
calls. Does not edit the existing course banks, textures or captured assemblies.
"""
import argparse
import bisect
import hashlib
import json
import math
import struct
from pathlib import Path

CANONICAL = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
BASE = 0x0c020000
COURSES = ['k_ez', 's_nm', 'h_hd', 'k_df', 's_vh', 's_uh', 'n_sy', 'k_tu', 'k_df3']
# Each tuple is (constructor, path-table address, count, pointer table).
OWNERS = {
    2: ((0x0c19fde0, 0x0c33be04, 4, True), (0x0c1a0a80, 0x0c33be14, 4, True)),
    3: ((0x0c19d1a0, 0x0c2a357b, 4, False), (0x0c19ddc0, 0x0c2a37fb, 4, False)),
    5: ((0x0c1a4480, 0x0c33be24, 4, True), (0x0c1a50a0, 0x0c33be34, 4, True)),
    6: ((0x0c1a2b80, 0x0c2a4787, 6, False), (0x0c1a36e0, 0x0c2a4a9f, 6, False)),
    7: ((0x0c1a6180, 0x0c2a536f, 2, False), (0x0c1a6e40, 0x0c2a5577, 2, False)),
    8: ((0x0c19ddc0, 0x0c2a37fb, 4, False), (0x0c19ddc0, 0x0c2a37fb, 4, False)),
}


def integer(value):
    return int(value, 0) if isinstance(value, str) else value


def source_paths(code, descriptor):
    _, table, count, pointers = descriptor
    result = []
    for index in range(count):
        address = struct.unpack_from('<I', code, table - BASE + index * 4)[0] if pointers else table + index * 64
        offset = address - BASE
        value = code[offset:code.index(0, offset)].decode('ascii')
        if not value.startswith('/driveA/tree/') or not value.endswith('.bin'):
            raise ValueError('Original tree resource table changed')
        result.append((address, value))
    return result


def read_owners(code, hostfs, descriptor, constructor, boundaries):
    expected = source_paths(code, descriptor)
    if len(constructor['owners']) != len(expected):
        raise ValueError('Captured original constructor owner count mismatch')
    owners = []
    for capture, (address, path) in zip(constructor['owners'], expected):
        if capture['path'] != path:
            raise ValueError('Captured original placement path mismatch')
        settings = [integer(v) for v in capture['settings_words']]
        if len(settings) != 24:
            raise ValueError('Expected original 96-byte spatial settings')
        raw = (hostfs / path[len('/driveA/'):]).read_bytes()
        if not raw or len(raw) % 40 or len(raw) // 40 > 20000:
            raise ValueError('Original placement record extent mismatch')
        records = []
        for offset in range(0, len(raw), 40):
            words = struct.unpack_from('<10I', raw, offset)
            floats = struct.unpack_from('<3f', raw, offset + 4)
            scale = struct.unpack_from('<f', raw, offset + 24)[0]
            if not all(math.isfinite(v) for v in (*floats, scale)) or scale <= 0 or words[9] != 0:
                raise ValueError('Invalid original placement values')
            #084CB8..084D08: nonzero source flag sends Akagi placements to
            #the source sector containing record+32, capped to 31 groups.
            sector = min(31, bisect.bisect_right(boundaries, words[8]))
            records.append((words, sector))
        owners.append({'path': path, 'source_address': address, 'source_sha256': hashlib.sha256(raw).hexdigest(),
                       'sector_mode': integer(capture['sector_mode']), 'settings_words': settings,
                       'records': records})
    return owners


def write_sidecar(path, capture, owners, chunk_count):
    selections = capture['object_selections']
    changes = capture['object_changes']
    static_counts = capture['base_static_instance_counts']
    if len(static_counts) != len(selections) or not changes or changes[0][0] != 0:
        raise ValueError('Invalid original object selection metadata')
    data = bytearray(b'ID3OBJ1\0' + struct.pack('<5I', 1, capture['path_count'], len(owners), len(selections), len(changes)))
    for owner in owners:
        data.extend(struct.pack('<25I', owner['sector_mode'], *owner['settings_words']))
        data.extend(struct.pack('<I', len(owner['records'])))
        for words, sector in owner['records']:
            data.extend(struct.pack('<11I', *words, sector))
    for draws, static_count in zip(selections, static_counts):
        data.extend(struct.pack('<2I', static_count, len(draws)))
        previous = 0
        for draw in draws:
            owner = owners[draw['owner']]
            if not previous <= draw['before'] <= static_count:
                raise ValueError('Original object call order outside static assembly')
            if any(words[0] + draw['chunk_base'] >= chunk_count for words, _ in owner['records']):
                raise ValueError('Original placed chunk outside selected course bank')
            data.extend(struct.pack('<3I4i', draw['owner'], draw['chunk_base'], draw['before'], *draw['ranges']))
            previous = draw['before']
    previous = -1
    for start, selection in changes:
        if not previous < start < capture['path_count'] or not 0 <= selection < len(selections):
            raise ValueError('Original object transition bounds')
        data.extend(struct.pack('<2I', start, selection)); previous = start
    pending = path.with_suffix('.objects-pending')
    pending.write_bytes(data); pending.replace(path)
    return hashlib.sha256(data).hexdigest()


def export(image, hostfs, project, constructor_capture, capture_dir, proof, courses):
    code = image.read_bytes()
    if hashlib.sha256(code).hexdigest() != CANONICAL:
        raise ValueError('Canonical source image mismatch')
    constructors = json.loads(constructor_capture.read_text())
    if isinstance(constructors, dict):
        constructors = constructors['constructors']
    constructors = {integer(item['entry']): item for item in constructors}
    report = {'schema': 'idas3-original-course-objects-v1', 'source_image_sha256': CANONICAL,
              'owner_constructor': '0C083FC0', 'owner_vtable': '0C384B5C', 'placement_loader': '0C084740',
              'spatial_setup': '0C084780', 'position_update': '0C0842E0', 'render': '0C084320',
              'draw_transform': '0C085260: T(position) * Ry(phase16) * Rx(phase20) * S(scale24)',
              'asset_policy': 'Existing selected course chunks/materials/textures, original 40-byte placement records. No generated scenery.',
              'renderer_boundary': 'Source spatial/sector admission and draw ordering; downstream source model-frustum helper is handled by native geometry clipping.',
              'variants': []}
    for ci in courses:
        cid = COURSES[ci]
        folder = project / 'data/original_models/courses' / cid
        manifest = json.loads((folder / 'banks_manifest.json').read_text())
        for variant in manifest['variants']:
            night, wet = variant['night'], variant.get('weather', 0)
            descriptor = OWNERS[ci][int(bool(night or wet))] if ci in OWNERS else None
            for reverse in (0, 1):
                name = ('night' if night else 'day') + ('_reverse' if reverse else '_forward') + ('_wet' if wet else '')
                path = capture_dir / f'{cid}_{name}.json'
                capture = json.loads(path.read_text())
                if capture['scene_index'] != ci * 2 + night or capture['reverse'] != reverse:
                    raise ValueError('Wrong original object selector variant')
                owners = read_owners(code, hostfs, descriptor, constructors[descriptor[0]], capture['boundaries']) if descriptor else []
                target = folder / f'scene_{name}.idasobjects'
                digest = write_sidecar(target, capture, owners, variant['world_chunk_count'])
                report['variants'].append({'course': cid, 'variant': name,
                    'constructor': hex(descriptor[0]) if descriptor else None,
                    'owners': [{k: v for k, v in owner.items() if k != 'records'} | {'placements': len(owner['records'])} for owner in owners],
                    'placements': sum(len(owner['records']) for owner in owners),
                    'selections': len(capture['object_selections']), 'transitions': len(capture['object_changes']),
                    'file': target.relative_to(project).as_posix(), 'sha256': digest})
                print(f'Exported {cid} {name}: {sum(len(owner["records"]) for owner in owners)} original placements', flush=True)
    proof.mkdir(parents=True, exist_ok=True)
    (proof / 'tree-export-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('image', 'hostfs', 'project', 'constructor-capture', 'capture-dir', 'proof'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--courses', default='0,1,2,3,4,5,6,7,8')
    args = parser.parse_args()
    export(args.image, args.hostfs, args.project, args.constructor_capture, args.capture_dir, args.proof,
           [int(value) for value in args.courses.split(',')])
