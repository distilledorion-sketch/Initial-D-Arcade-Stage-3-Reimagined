"""Export original front/rear lamp-trail art and the35 car anchor records.

No runtime positions, dimensions, colors or streak textures are synthesized.
1A9CE0 applies the source0.3Y lift and0.5*0.8 width scale at runtime.
"""
import argparse
import hashlib
import json
import struct
import tempfile
from pathlib import Path
from extract_original_models import parse_model, read_payload, write_binary
from export_original_assembly import IMAGE_HASH
import texture_bank


def export(hostfs, image, output):
    raw = image.read_bytes()
    if hashlib.sha256(raw).hexdigest() != IMAGE_HASH:
        raise ValueError('Canonical original executable mismatch')
    source = hostfs / 'model/effect/taillight'
    _, chunks, files, payload = parse_model(source)
    if len(chunks) != 8:
        raise ValueError('Expected eight authored lamp-trail sprite chunks')
    records = []
    for car in range(35):
        pointer, width, rear, front = struct.unpack_from('<IfII', raw, 0x33c5f4-0x20000+car*16)
        if rear not in (2, 4) or front not in (2, 4) or not 0 < width < 1:
            raise ValueError('Unexpected original lamp-trail record')
        points = raw[pointer-0xc020000:pointer-0xc020000+12*(rear+front)]
        if len(points) != 12*(rear+front):
            raise ValueError('Truncated original lamp anchors')
        live_pointer, *live_values = struct.unpack_from('<IfII', raw, 0x337ad0-0x20000+car*16)
        if live_values != [width, rear, front] or raw[live_pointer-0xc020000:live_pointer-0xc020000+len(points)] != points:
            raise ValueError('Driving/replay anchor tables differ')
        records.append((width, rear, front, points))
    output.mkdir(parents=True, exist_ok=True)
    write_binary(output / 'taillight.idasmesh', chunks)
    with tempfile.TemporaryDirectory(prefix='idas3-lamp-texture-') as temp:
        decoded = Path(temp) / 'taillight.bin'
        decoded.write_bytes(read_payload(source / 'taillight_tex.bin.nz'))
        bank = texture_bank.export_bank(source / 'taillight_tex.tbl', decoded, output / 'textures', 'twiddled')
    if len(bank['textures']) != 2:
        raise ValueError('Expected two original lamp textures')
    with (output / 'anchors.bin').open('wb') as f:
        f.write(b'ID3LTR1\0'); f.write(struct.pack('<I', len(records)))
        for width, rear, front, points in records:
            f.write(struct.pack('<fII', width, rear, front)); f.write(points)
    report = dict(schema='idas3-original-lamp-trails-v1', source_image_sha256=IMAGE_HASH,
                  source='HOSTFS/model/effect/taillight', anchor_table='0C33C5F4',
                  equivalent_driving_anchor_table='0C337AD0',
                  owner='0C1A9CE0', lamp='0C1A9320', strip='0C1A91A0',
                  chunks=8, textures=2, cars=35,
                  decoded_polygon_sha256=hashlib.sha256(payload).hexdigest(),
                  source_sha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.iterdir()) if p.is_file()},
                  outputs={str(p.relative_to(output)):dict(bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest())
                           for p in (output/'taillight.idasmesh',output/'anchors.bin',output/'textures/textures.idastex')},
                  scope='Unchanged original lamp art, material words and body-local anchors. Runtime visibility, camera history and integration require separate verification.')
    (output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:report[k] for k in ('chunks','textures','cars','outputs')},indent=2))


if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('hostfs','image','out'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();export(a.hostfs,a.image,a.out)
