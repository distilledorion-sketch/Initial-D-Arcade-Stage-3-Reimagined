#!/usr/bin/env python3
"""Extract the pre-race loading screens, their common overlay and the VS screen.

`/driveA/model/loading` holds sixteen authored banks: `load00`..`load12` are the
thirteen artworks the game picks between (its own log line is "Load BG Number:
%d"), `loadcmn` is the furniture drawn over whichever one is chosen, `loadwait`
is the waiting mark and `loadvs` is the versus screen. Each is an ordinary
polygon/texture pair, so this reuses the model and texture-bank readers rather
than introducing a second decoder.

    python extract_original_loading.py --source <HOSTFS>/model/loading \
        --out <native>/data/original_assets/loading --image <idas3_main_0C020000.bin>

The artwork stays private original material; nothing here invents layout.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import struct
from pathlib import Path

import texture_bank
import extract_original_models as models

# The thirteen artworks are 8bpp palettised; their palette has not been
# located yet, and this importer never guesses one. The overlay, the waiting
# mark and the versus screen are ordinary ARGB4444 and import as they are.
ARTWORK = [f'load{index:02d}' for index in range(13)]
# load10 alone is RGB565 rather than palettised, so it imports today.
DIRECT_ARTWORK = ['load10']
BANKS = ARTWORK + ['loadcmn', 'loadwait', 'loadvs']


def payload_for(folder: Path, stem: str) -> Path:
    plain, packed = folder / f'{stem}.bin', folder / f'{stem}.bin.nz'
    if plain.exists():
        if packed.exists() and plain.read_bytes() != packed.read_bytes():
            raise ValueError(f'Ambiguous differing .bin/.bin.nz copies: {stem}')
        return plain
    if packed.exists():
        return packed
    raise ValueError(f'Missing extracted payload: {stem}')


def export_bank(folder: Path, prefix: str, output: Path, layout: str, image: Path | None = None) -> dict:
    name, chunks, sources, polygon = models.parse_model(
        folder, polygon_table=f'{prefix}_pol.tbl', texture_table_name=f'{prefix}_tex.tbl')
    if name != prefix:
        raise ValueError(f'Polygon table named {name}, expected {prefix}')
    output.mkdir(parents=True, exist_ok=True)
    texture_payload = payload_for(folder, f'{prefix}_tex')
    raw = models.read_payload(texture_payload)
    texture_input = texture_payload
    if raw != texture_payload.read_bytes():
        texture_input = output / 'original_texture_decoded.bin'
        texture_input.write_bytes(raw)
    metadata=sources['texture_table'].read_bytes()
    if len(metadata)==16 and metadata[4]==7:
        from export_original_assembly import IMAGE_HASH
        if image is None: raise ValueError('Canonical program image required for loading palettes')
        program=image.read_bytes()
        if hashlib.sha256(program).hexdigest()!=IMAGE_HASH: raise ValueError('Canonical palette image identity mismatch')
        number=int(prefix[4:]);base=0x0c020000
        # 16CEB8..16CEC6 indexes 28B8F8 by selected Load BG Number,
        # then calls191A40(palette ID,1). No palette search or inference.
        palette_id=struct.unpack_from('<I',program,0x0c28b8f8-base+number*4)[0]
        table=struct.unpack_from('<I',program,0x0c191aa8-base)[0]
        address=struct.unpack_from('<I',program,table-base+palette_id*4)[0]
        palette=program[address-base:address-base+1024];colors=struct.unpack('<256I',palette)
        w,h,fmt,flags,reserved,offset,tail=struct.unpack('<HHBBHII',metadata)
        if (w,h,fmt,flags,reserved,offset,tail)!=(512,512,7,7,0,0,0) or len(raw)!=w*h: raise ValueError('Loading PAL8 layout changed')
        rgba=bytearray(w*h*4)
        for y in range(h):
            for x in range(w):
                argb=colors[raw[texture_bank.texel_index(x,y,w,h,layout)]];at=(y*w+x)*4
                rgba[at:at+4]=bytes(((argb>>16)&255,(argb>>8)&255,argb&255,argb>>24))
        td=output/'textures';td.mkdir(parents=True,exist_ok=True)
        texture_bank.write_native_pack(td/'textures.idastex',[{'index':0,'width':w,'height':h,'rgba':rgba}])
        texture_bank.write_png(td/'texture_000.png',w,h,rgba)
        (td/'original_indices.pal8').write_bytes(raw);(td/'original_palette.argb32').write_bytes(palette)
        textures={'textures':[{'index':0,'width':w,'height':h,'rgba_sha256':hashlib.sha256(rgba).hexdigest()}],
            'palette_id':palette_id,'palette_address':hex(address),'palette_sha256':hashlib.sha256(palette).hexdigest(),
            'source_image_sha256':IMAGE_HASH,'evidence':'16CEB8..16CEC6 selects28B8F8[Load BG Number];191A40 uploads original ARGB8888 palette.'}
        (td/'manifest.json').write_text(json.dumps(textures,indent=2)+'\n')
    else:
        textures = texture_bank.export_bank(sources['texture_table'], texture_input,output / 'textures', layout)
    models.write_binary(output / f'{prefix}.idasmesh', chunks)
    (output / 'original_pol.bin').write_bytes(polygon)
    (output / 'original_pol.tbl').write_bytes(sources['table'].read_bytes())
    manifest = {
        'schema': 'idas3-original-loading-bank-v1',
        'name': prefix,
        'private_original_assets': True,
        'source_directory': str(folder.resolve()),
        'layout': layout,
        'source_files': {key: {'path': str(path.resolve()), 'bytes': path.stat().st_size,
                               'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}
                         for key, path in sources.items() if path.exists()},
        'decoded_polygon_sha256': hashlib.sha256(polygon).hexdigest(),
        'chunk_count': len(chunks),
        'texture_count': len(textures['textures']) if isinstance(textures, dict) else len(textures),
    }
    (output / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    return manifest


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source', type=Path, required=True)
    ap.add_argument('--out', type=Path, required=True)
    ap.add_argument('--image',type=Path,required=True)
    ap.add_argument('--layout', default='twiddled')
    args = ap.parse_args()
    banks = []
    for prefix in BANKS:
        if not (args.source / f'{prefix}_pol.tbl').exists():
            raise SystemExit(f'Missing authored bank: {prefix}')
        banks.append(export_bank(args.source, prefix, args.out / prefix, args.layout,args.image))
    index = {'schema': 'idas3-original-loading-index-v1', 'private_original_assets': True,
             'source': str(args.source.resolve()), 'layout': args.layout,
             'artwork_banks_pending_palette': [],
             'artwork_banks_decoded': ARTWORK,
             'overlay_bank': 'loadcmn', 'wait_bank': 'loadwait', 'versus_bank': 'loadvs',
             'banks': [{'name': b['name'], 'chunks': b['chunk_count'],
                        'textures': b['texture_count']} for b in banks]}
    (args.out / 'index.json').write_text(json.dumps(index, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'banks': len(banks),
                      'textures': sum(b['texture_count'] for b in banks),
                      'chunks': sum(b['chunk_count'] for b in banks)}, indent=2))


if __name__ == '__main__':
    main()
