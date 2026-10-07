"""Export the original cup, sixty water frames and six splash frames unchanged."""
import argparse
import hashlib
import json
from pathlib import Path
from extract_original_models import parse_model, read_payload, write_binary
import texture_bank

def export(hostfs, output):
    source = hostfs / 'model/cupwater'
    _, chunks, _, _ = parse_model(source)
    if len(chunks) != 70:
        raise ValueError('Expected the original 70-piece cupwater bank')
    output.mkdir(parents=True, exist_ok=True)
    write_binary(output / 'cupwater.idasmesh', chunks)
    payload = read_payload(source / 'cupwater_tex.bin.nz')
    decoded = output / 'decoded_texture.bin'
    decoded.write_bytes(payload)
    textures = texture_bank.export_bank(source / 'cupwater_tex.tbl', decoded, output / 'textures', 'twiddled')
    decoded.unlink()
    texture_manifest = output / 'textures/manifest.json'
    texture_manifest.write_text(texture_manifest.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
    manifest = {
        'source': 'HOSTFS/model/cupwater',
        'chunks': len(chunks), 'textures': len(textures['textures']),
        'geometry_uv_transform': 'none',
        'draw_owner': '17D020', 'motion_owner': '17D2A0', 'motion_curves': '323BE4',
        'source_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(source.iterdir()) if p.is_file()},
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2)+'\n', encoding='utf-8', newline='\n')
    print(json.dumps(manifest, indent=2))

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    export(args.hostfs, args.out)
