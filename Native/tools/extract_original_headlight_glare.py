"""Export the original hilight sprites and all35 cars' lamp anchor records."""
import argparse
import hashlib
import json
import struct
from pathlib import Path
from extract_original_models import parse_model, read_payload, write_binary
import texture_bank

def export(hostfs, image, output):
    source = hostfs / 'model/hilight'
    _, chunks, _, _ = parse_model(source)
    if len(chunks) != 4:
        raise ValueError('Expected four original hilight sprites')
    output.mkdir(parents=True, exist_ok=True)
    write_binary(output / 'hilight.idasmesh', chunks)
    decoded = output / 'decoded_texture.bin'
    decoded.write_bytes(read_payload(source / 'hilight_tex.bin.nz'))
    bank = texture_bank.export_bank(source / 'hilight_tex.tbl', decoded, output / 'textures', 'twiddled')
    decoded.unlink()
    texture_manifest = output / 'textures/manifest.json'
    texture_manifest.write_text(texture_manifest.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
    data = image.read_bytes()
    with (output / 'anchors.bin').open('wb') as out:
        out.write(b'ID3HLG1\0')
        out.write(struct.pack('<I', 35))
        for car in range(35):
            pointer, count, style = struct.unpack_from('<3I', data, 0x0c2fc2e8 - 0x0c020000 + car*12)
            if count not in (2, 4) or style >= len(chunks):
                raise ValueError('Unexpected original lamp anchor record')
            out.write(struct.pack('<II', count, style))
            out.write(data[pointer-0x0c020000:pointer-0x0c020000+count*12])
    manifest = {
        'source': 'HOSTFS/model/hilight', 'anchor_table': '0C2FC2E8',
        'factory': '0C0D2E20', 'visibility': '0C0D36A0', 'draw': '0C0D40A0',
        'chunks': len(chunks), 'textures': len(bank['textures']), 'cars': 35,
        'source_image_sha256': hashlib.sha256(data).hexdigest(),
        'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.iterdir()) if p.is_file()},
        'geometry_uv_transform': 'none; anchors remain body-local; host body pose includes source ride-height offset',
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8', newline='\n')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs', type=Path, required=True)
    parser.add_argument('--image', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    export(args.hostfs, args.image, args.out)
