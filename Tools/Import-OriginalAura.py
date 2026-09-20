#!/usr/bin/env python3
"""Import AS3's authored aura geometry, color animation and source tables.

No guest execution, texture synthesis, vertex recoloring or save access.
--check regenerates every expected byte in memory and verifies the import.
"""
from pathlib import Path
import argparse, hashlib, json, struct, sys

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(PROJECT.parent / 'InitialDRemake/tools'))
from extract_original_models import parse_model, read_payload
from texture_bank import decode

IMAGE_SHA = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
DEFAULT_FILES = Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files')
COLORS = ('blue','green','crimson','orange','red','yellow','purple','cyan','white','spa')

def sha(b): return hashlib.sha256(b).hexdigest()

def mesh_pack(chunks):
    b = bytearray(b'IDAS3M1\0' + struct.pack('<II', 1, len(chunks)))
    for c in chunks:
        b += struct.pack('<4I', c['index'], c['source_offset'], c['source_size'], len(c['batches']))
        b += struct.pack('<24I', *c['header'])
        for q in c['batches']:
            b += struct.pack('<4I', q['source_offset'], len(q['vertices']), len(q['indices']), len(q['source_vertex_bytes']))
            b += struct.pack('<8I', *q['words']) + struct.pack('<16I', *q['material'])
            for v in q['vertices']: b += struct.pack('<I8f2I', *v)
            b += struct.pack(f'<{len(q["indices"])}I', *q['indices']) + q['source_vertex_bytes']
    return bytes(b)

def generate(files):
    host = files / 'driveA/HOSTFS'
    image_path = files / 'idas3_main_0C020000.bin'
    image = image_path.read_bytes()
    if len(image) != 4194304 or sha(image) != IMAGE_SHA: raise ValueError('Canonical original image mismatch')
    source = host / 'model/effect/aura'
    _, chunks, _, polygon = parse_model(source)
    if len(chunks) != 1 or len(chunks[0]['batches']) != 1: raise ValueError('Unexpected aura geometry')
    batch = chunks[0]['batches'][0]
    if len(batch['vertices']) != 3474 or len(batch['indices']) != 10134 or batch['words'][6] != 0x4a:
        raise ValueError('Unexpected original aura vertex layout')
    out = {'aura.idasmesh': mesh_pack(chunks)}
    sources = {}
    for p in sorted(source.iterdir()):
        if p.is_file():
            b = p.read_bytes(); out['source/' + p.name] = b
            sources[p.name] = {'path': str(p), 'bytes': len(b), 'sha256': sha(b)}
    for name in COLORS:
        p = host / 'vtxcolor' / (name + '_vtx.bin'); b = p.read_bytes()
        if len(b) != 30 * 1788 * 4: raise ValueError('Original color frame count mismatch')
        out['colors/' + p.name] = b
        sources[p.name] = {'path': str(p), 'bytes': len(b), 'sha256': sha(b)}
    def region(address, size): return image[address - 0x0c020000:address - 0x0c020000 + size]
    mapping = region(0x0c320040, 3474 * 4)
    if max(struct.unpack('<3474I', mapping)) != 1787: raise ValueError('Original aura color remap mismatch')
    out['vertex_color_indices.bin'] = mapping
    out['car_dimensions.bin'] = region(0x0c28d890, 35 * 44)
    table = (source / 'aura_tex.tbl').read_bytes()
    raw = read_payload(source / 'aura_tex.bin.nz')
    w,h,fmt,flags,reserved,offset,tail = struct.unpack('<HHBBHII', table)
    rgba = decode(raw[offset:offset+w*h*2], w, h, fmt, 'twiddled')
    out['textures.idastex'] = b'IDAS3T1\0' + struct.pack('<II4I',1,1,0,w,h,len(rgba)) + rgba
    # All actual effect submissions are untextured. Preserve its original
    # texture bank as provenance, without assigning it to the color mesh.
    manifest = {
        'schema': 'idas3-original-aura-v1', 'sourceImage': str(image_path), 'sourceImageSha256': IMAGE_SHA,
        'sourceFiles': sources, 'vertices': 3474, 'triangles': 3378,
        'colorFrames': 30, 'colorsPerFrame': 1788, 'colorByteOrder': 'big-endian ARGB',
        'paletteOrder': list(COLORS), 'imageTables': {
            'vertex_color_indices.bin': {'address':'0C320040','bytes':len(mapping)},
            'car_dimensions.bin': {'address':'0C28D890','rows':35,'rowBytes':44}},
        'runtimeMaterial': {'pcw':'8a00072f','isp':'91c00000','tsp':'849804c0','gmp':'000006aa'},
        'sourceOwners': ['0C17B0A0','0C17B7E0','0C17B900'],
        'files': {n: {'bytes':len(b),'sha256':sha(b)} for n,b in sorted(out.items())}}
    out['manifest.json'] = (json.dumps(manifest,indent=2)+'\n').encode()
    return out

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--source-files',type=Path,default=DEFAULT_FILES)
    ap.add_argument('--out',type=Path,default=PROJECT/'Native/data/original_assets/aura')
    ap.add_argument('--check',action='store_true')
    a=ap.parse_args(); files=generate(a.source_files)
    for name,data in files.items():
        path=a.out/name
        if a.check:
            if not path.is_file() or path.read_bytes()!=data: raise ValueError('Import differs: '+name)
        else:
            path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
    print(json.dumps({'passed':True,'checkOnly':a.check,'files':len(files),'vertices':3474,'colorFrames':30,'palettes':10}))

if __name__=='__main__': main()
