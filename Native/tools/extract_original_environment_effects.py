"""Recover leaf/flare meshes, textures and source course visibility tables."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from extract_original_models import parse_model, read_payload, write_binary
from texture_bank import decode, write_native_pack, write_png

def extract(hostfs, image, out):
    raw = image.read_bytes()
    if hashlib.sha256(raw).hexdigest() != 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335':
        raise ValueError('Expected the verified AS3 source image')
    report = {'schema': 'idas3-environment-effects-v1', 'image_sha256': hashlib.sha256(raw).hexdigest(), 'banks': {}}
    for name, relative in [('leaf', 'model/leaf'), ('flare', 'model/effect/flare')]:
        source, target = hostfs / relative, out / name
        target.mkdir(parents=True, exist_ok=True)
        _, chunks, _, _ = parse_model(source)
        layouts = {}
        for chunk in chunks:
            for batch in chunk['batches']:
                for slot, word in [(9, 3), (13, 5)]:
                    index = batch['material'][slot]
                    if index == 0xffffffff:
                        continue
                    tcw = batch['words'][word]
                    layout = ('linear' if tcw & (1 << 26) else 'twiddled', (tcw >> 27) & 7)
                    if index in layouts and layouts[index] != layout:
                        raise ValueError('Conflicting texture layout')
                    layouts[index] = layout
        table = (source / (name + '_tex.tbl')).read_bytes()
        payload = read_payload(source / (name + '_tex.bin.nz'))
        records = []
        for index, (w, h, fmt, flags, _, offset, _) in enumerate(struct.iter_unpack('<HHBBHII', table)):
            layout, material_format = layouts[index]
            if material_format != fmt:
                raise ValueError('Texture format mismatch')
            rgba = decode(payload[offset:offset+w*h*2], w, h, fmt, layout)
            write_png(target / f'texture_{index:03}.png', w, h, rgba)
            records.append(dict(index=index, width=w, height=h, format=fmt, flags=flags,
                                offset=offset, layout=layout, rgba=rgba))
        write_native_pack(target / 'textures.idastex', records)
        write_binary(target / (name + '.idasmesh'), chunks)
        report['banks'][name] = {'chunks': len(chunks), 'textures': len(records),
            'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.iterdir()) if p.is_file()}}
    def unpack(address, fmt):
        return struct.unpack_from('<' + fmt, raw, address - 0x0c020000)
    # 180B00 indexes the original per-course path visibility as uint32 words.
    tables = bytearray(b'IDASENV1')
    tables += raw[0x28e650-0x20000:0x28e650-0x20000+8*12]
    tables += raw[0x28e5c0-0x20000:0x28e5c0-0x20000+12*12]
    for course in range(8):
        ptr, = unpack(0x0c337358 + course*4, 'I')
        last, = unpack(0x0c337378 + course*4, 'I')
        values = unpack(ptr, 'I'*(last+1))
        if any(v not in (0, 1) for v in values):
            raise ValueError(f'Invalid visibility table for course {course}')
        tables += struct.pack('<I', last+1) + bytes(values)
    (out / 'environment.bin').write_bytes(tables)
    (out / 'manifest.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps(report, indent=2))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('hostfs', 'image', 'out'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    extract(args.hostfs, args.image, args.out)
