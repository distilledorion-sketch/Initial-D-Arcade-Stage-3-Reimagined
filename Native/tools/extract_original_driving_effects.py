"""Extract original backfire, smoke and wet tire-trail banks without replacement art.

This identifies assets; it does not claim recovered timing or a dry skid decal.
Run against a legally supplied HOSTFS directory. Output is private game data.
"""
import argparse
import hashlib
import json
import shutil
import struct
from pathlib import Path
from extract_original_models import parse_model, read_payload, write_binary
from texture_bank import decode, write_native_pack, write_png


def extract(hostfs: Path, output: Path):
    report = {'schema': 'idas3-driving-effect-assets-v1', 'banks': {},
              'dry_skid_marks': 'Unconfirmed: smoke and rainmark are not evidence of a rubber skid decal.',
              'runtime_integration': False}
    for name in ('bkfire', 'smoke', 'rainmark'):
        source = hostfs / 'model/effect' / name
        target = output / name
        target.mkdir(parents=True, exist_ok=True)
        _, chunks, _, _ = parse_model(source)
        layouts = {}
        for chunk in chunks:
            for batch in chunk['batches']:
                # Original submitted texture-control words: bit26 scan order,
                # bits27..29 pixel format. Match each active texture reference.
                for slot, tcw in ((9, 3), (13, 5)):
                    index = batch['material'][slot]
                    if index == 0xffffffff:
                        continue
                    word = batch['words'][tcw]
                    layout = 'linear' if word & (1 << 26) else 'twiddled'
                    value = (layout, (word >> 27) & 7)
                    if index in layouts and layouts[index] != value:
                        raise ValueError('Conflicting source material texture layouts')
                    layouts[index] = value
        table = (source / (name + '_tex.tbl')).read_bytes()
        texture_file = source / (name + '_tex.bin.nz')
        if not texture_file.exists():
            texture_file = source / (name + '_tex.bin')
        payload = read_payload(texture_file)
        records = []
        for index, (w, h, fmt, flags, reserved, offset, tail) in enumerate(struct.iter_unpack('<HHBBHII', table)):
            layout, material_format = layouts[index]
            if material_format != fmt:
                raise ValueError('Source material/table pixel formats disagree')
            raw = payload[offset:offset + w*h*2]
            rgba = decode(raw, w, h, fmt, layout)  # Includes exact 16-bit repack checks.
            file = f'texture_{index:03}.png'
            write_png(target / file, w, h, rgba)
            records.append(dict(index=index, width=w, height=h, format=fmt,
                                flags=flags, offset=offset, layout=layout, file=file,
                                source_texels_sha256=hashlib.sha256(raw).hexdigest(),
                                rgba_sha256=hashlib.sha256(rgba).hexdigest(), rgba=rgba))
        write_native_pack(target / 'textures.idastex', records)
        write_binary(target / (name + '.idasmesh'), chunks)
        sources = {}
        for file in source.iterdir():
            if file.is_file():
                shutil.copy2(file, target / file.name)
                sources[file.name] = hashlib.sha256(file.read_bytes()).hexdigest()
        for record in records:
            del record['rgba']
        report['banks'][name] = dict(source=str(source.resolve()), source_sha256=sources,
                                    chunks=len(chunks), textures=records,
                                    layout_evidence='Original per-batch TCW scan-order and pixel-format bits; exact texel decode/repack.',
                                    timing='Not recovered by this extractor')
    (output / 'manifest.json').write_text(json.dumps(report, indent=2))
    return report


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    result = extract(args.hostfs, args.out)
    print(json.dumps({name: {'textures': len(bank['textures']), 'chunks': bank['chunks']}
                      for name, bank in result['banks'].items()}))
