#!/usr/bin/env python3
"""Recover omitted spectator lists through the original course selectors.

Constructor traces supply file-to-field mappings, including absent source
files (which remain null, as in the original loader). Course selectors run at
every authoring-path index; frustum admission is delegated to the renderer.
Existing scene banks, textures, static assemblies and lamps remain unchanged.
"""
import argparse
import hashlib
import json
import struct
import subprocess
from pathlib import Path
from export_original_assembly import IMAGE_HASH

IDS = ('k_ez', 's_nm', 'h_hd', 'k_df', 's_vh', 's_uh', 'n_sy', 'k_tu', 'k_df3')
CTORS = (
    (0x0C19EA40, 0x0C19F4C0), (0x0C1A18C0, 0x0C1A2160),
    (0x0C19FDE0, 0x0C1A0A80), (0x0C19D1A0, 0x0C19DDC0),
    (0x0C040AE0, 0x0C040AE0), (0x0C1A4480, 0x0C1A50A0),
    (0x0C1A2B80, 0x0C1A36E0), (0x0C1A6180, 0x0C1A6E40),
    (0x0C19DDC0, 0x0C19DDC0),
)


def source_lists(trace, hostfs):
    """Select only positional crowd and already implemented lamp lists."""
    found, absent = [], []
    for f in trace['course_files']:
        name = f['path']
        if not any(word in name for word in ('_gal', '_human', '_hito', '_lamp')):
            continue
        if not name.startswith('/driveA/'):
            raise ValueError('Unexpected source position-list path')
        p = hostfs / name[len('/driveA/'):]
        if not p.is_file():
            absent.append(f)
            continue
        data = p.read_bytes()
        count, components = struct.unpack_from('<II', data)
        if components != 3 or len(data) != 8 + 12 * count:
            raise ValueError(f'Position-list header/extent: {p}')
        found.append(dict(offset=f['offset'], path=p.resolve().as_posix(),
                          existing='_lamp' in name, count=count,
                          source_path=name, sha256=hashlib.sha256(data).hexdigest()))
    if len({f['offset'] for f in found}) != len(found):
        raise ValueError('Duplicate constructor field')
    return found, absent


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ('image', 'hostfs', 'project', 'capture_exe', 'verification'):
        p.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    p.add_argument('--constructors', type=Path, action='append', required=True)
    p.add_argument('--courses', default=','.join(map(str, range(9))))
    a = p.parse_args()
    if hashlib.sha256(a.image.read_bytes()).hexdigest() != IMAGE_HASH:
        raise ValueError('Canonical image mismatch')
    traces = {}
    for path in a.constructors:
        for trace in json.loads(path.read_text())['constructors']:
            if trace['entry'] in traces:
                raise ValueError('Duplicate constructor trace')
            traces[trace['entry']] = trace
    a.verification.mkdir(parents=True, exist_ok=True)
    records = []
    for ci in map(int, a.courses.split(',')):
        cid = IDS[ci]
        base = a.project / 'data/original_models/courses' / cid
        banks = json.loads((base / 'banks_manifest.json').read_text())
        for variant in banks['variants']:
            night, wet = variant['night'], variant.get('weather', 0)
            # The original dispatcher selects the following night constructor
            # for daytime rain, except Happogahara; Snow always uses night.
            ctor = CTORS[ci][int(bool(night or (wet and ci != 4)))]
            lists, absent = source_lists(traces[ctor], a.hostfs)
            for reverse in (0, 1):
                name = ('night' if night else 'day') + ('_reverse' if reverse else '_forward') + ('_wet' if wet else '')
                old = json.loads((base / (name + '_capture.json')).read_text())
                seed = a.verification / (cid + '_' + name + '.seeds.txt')
                seed.write_text('\n'.join(f"{r['offset']} {int(r['existing'])} {json.dumps(r['path'])}" for r in lists) + '\n')
                capture = a.verification / (cid + '_' + name + '.json')
                subprocess.run([str(a.capture_exe), str(a.image), str(ci * 2 + night), str(reverse),
                                str(old['path_count']), str(variant['world_chunk_count']), str(capture), str(wet), str(seed)], check=True)
                new = json.loads(capture.read_text())
                for field in ('assemblies', 'changes'):
                    if new['static_' + field] != old[field]:
                        raise ValueError(f'{cid} {name}: existing static {field} changed')
                if new['boundaries'] != old['boundaries']:
                    raise ValueError('Original course sector boundaries changed')
                if new['owner_before_billboard_ties'] and new['billboard_before_owner_ties']:
                    raise ValueError('Crowd/tree insertion order needs interleaved representation')
                destination = base / ('scene_' + name + '.idasbillboards')
                pending = destination.with_suffix('.pending')
                with pending.open('wb') as out:
                    out.write(b'ID3BLB1\0' + struct.pack('<IIII', 1, new['path_count'], len(new['assemblies']), len(new['changes'])))
                    for selection in new['assemblies']:
                        out.write(struct.pack('<I', len(selection)))
                        for draw in selection:
                            out.write(struct.pack('<II3f', draw['before'], draw['chunk'], *draw['position']))
                    for change in new['changes']:
                        out.write(struct.pack('<II', *change))
                pending.replace(destination)
                records.append(dict(course=cid, variant=name, constructor=ctor, source_lists=lists,
                                    absent_source_lists=absent, selections=len(new['assemblies']),
                                    maximum_draws=max(map(len, new['assemblies'])),
                                    billboard_before_owner_ties=new['billboard_before_owner_ties'],
                                    owner_before_billboard_ties=new['owner_before_billboard_ties'],
                                    chunks=sorted({d['chunk'] for s in new['assemblies'] for d in s}),
                                    original_instructions=new['original_instructions'],
                                    file=str(destination.relative_to(a.project)), sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
                print(f'Exported {cid} {name}: {records[-1]["maximum_draws"]} maximum original spectator draws; existing static assemblies identical.', flush=True)
    (a.verification / 'manifest.json').write_text(json.dumps(dict(
        schema='idas3-course-billboards-v1', source_image_sha256=IMAGE_HASH,
        capture_scope='Every source primary/static selector at every original authoring-path index; immutable position-list files from source constructor fields.',
        hooks=['Native matrix/graphics boundaries', 'Original world culling queries admit geometry for native view-frustum clipping', 'Unrelated dynamic tree/crow owners excluded'],
        records=records), indent=2) + '\n')


if __name__ == '__main__':
    main()
