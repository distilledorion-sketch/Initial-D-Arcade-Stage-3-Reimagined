"""Build private runtime tire/reflection textures from original HOSTFS data.

Keep the four original smoke images intact. The host alpha-blended particle
also needs a white coverage image: the source smoke material ignores texture
alpha and its black RGB border is not transparency. Use its authored grayscale
intensity as coverage, rather than displaying an opaque black square.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from texture_bank import decode, write_native_pack
from extract_original_models import read_payload


def export(hostfs, output):
    base = hostfs / 'model/effect/smoke'
    table = (base / 'smoke_tex.tbl').read_bytes()
    payload = base / 'smoke_tex.bin.nz'
    if not payload.exists(): payload = base / 'smoke_tex.bin'
    raw = read_payload(payload)
    assert len(table) == 64
    textures = []
    for i in range(4):
        width, height, fmt, flags, _, offset, _ = struct.unpack_from('<HHBBHII', table, i * 16)
        pixels = decode(raw[offset:offset + width * height * 2], width, height, fmt,
                        'linear' if i == 1 else 'twiddled')
        textures.append(dict(index=i, width=width, height=height, rgba=pixels))
    cloud = textures[1]
    coverage = bytearray(cloud['rgba'])
    for i in range(0, len(coverage), 4):
        r, g, b = coverage[i:i+3]
        assert r == g == b, 'Original smoke is a grayscale coverage image'
        x,y=(i//4)%cloud['width'],(i//4)//cloud['width']
        # The source cloud has nonzero RGB on its outer edges (up to170/255).
        # Its original draw owner is not a stock alpha-blended host quad.
        # Fade that boundary for this adapter, keeping the recovered interior.
        edge=min(1.0,min(x,y,cloud['width']-1-x,cloud['height']-1-y)/(cloud['width']*.15))
        coverage[i:i+4] = bytes((255, 255, 255, round(r*edge*edge*(3-2*edge))))
    textures.append(dict(index=4, width=cloud['width'], height=cloud['height'], rgba=bytes(coverage)))
    highlight = (hostfs / 'binary/j_env_dot128.bin.nz').read_bytes()
    assert len(highlight) == 128 * 128 * 2
    textures.append(dict(index=5, width=128, height=128, rgba=decode(highlight,128,128,1,'twiddled')))
    output.mkdir(parents=True,exist_ok=True)
    write_native_pack(output/'textures.idastex',textures)
    report = {'smoke_source_sha256': hashlib.sha256(raw).hexdigest(),
              'highlight_source_sha256': hashlib.sha256(highlight).hexdigest(),
              'runtime_sha256': hashlib.sha256((output/'textures.idastex').read_bytes()).hexdigest(),
              'scope': 'Original art with host smoke coverage/edge conversion and highlight binding. Effect timing, rubber geometry and race reflection binding are host presentation, not proven arcade controller parity.'}
    (output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    return report

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args()
    print(json.dumps(export(args.hostfs,args.out),indent=2))
