"""Export the three original SmokeDisp grid meshes without changing their art.

The source controller selects chunks 0/1/2 at 16/20/32 vertices per axis
(31DDA8). Their authored UVs encode grid indices, replaced at runtime by the
original U=.9 / V=density mapping. Texture 3 remains in the existing smoke
texture bank; this export does not regenerate or change that bank.
"""
import argparse
import hashlib
import json
from pathlib import Path
from extract_original_models import parse_model, write_binary


def export(hostfs: Path, output: Path):
    _, chunks, sources, payload = parse_model(hostfs / 'model/effect/smoke')
    if len(chunks) != 8:
        raise ValueError('Expected eight authored smoke chunks')
    selected = chunks[:3]
    records = []
    for chunk, dimension, count in zip(selected, (16, 20, 32), (480, 760, 1984)):
        if len(chunk['batches']) != 1:
            raise ValueError('Unexpected smoke grid batches')
        batch = chunk['batches'][0]
        if batch['words'][6] != 10 or len(batch['vertices']) != count:
            raise ValueError('Unexpected smoke grid vertex layout/count')
        if batch['material'][9] != 3 or batch['material'][13] != 3:
            raise ValueError('Unexpected smoke grid palette binding')
        cells = [(int(v[7]), int(-v[8])) for v in batch['vertices']]
        # The 32x32 source has small fractional residues in two V rows.
        # Preserve those floats in the asset. The source mapper uses FTRC;
        # do not round or rewrite UVs during export.
        if set(cells) != {(x, y) for y in range(dimension) for x in range(dimension)}:
            raise ValueError('Smoke grid does not cover the authored resolution')
        records.append(dict(chunk=chunk['index'], width=dimension, height=dimension,
                            vertices=count, triangles=len(batch['indices']) // 3,
                            source_offset=chunk['source_offset'], source_bytes=chunk['source_size']))
    output.mkdir(parents=True, exist_ok=True)
    path = output / 'smoke_grids.idasmesh'
    write_binary(path, selected)
    report = dict(schema='idas3-original-smoke-grids-v1',
                  source_polygon_sha256=hashlib.sha256(payload).hexdigest(),
                  source_table_sha256=hashlib.sha256(sources['table'].read_bytes()).hexdigest(),
                  runtime_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                  runtime_bytes=path.stat().st_size, grids=records,
                  scope='Original grid geometry, strip topology, normals, UV indices and material words. No geometry or texture simplification. Runtime controller integration and visual validation remain separate.')
    (output / 'smoke_grids_manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    print(json.dumps(export(args.hostfs, args.out), indent=2))
