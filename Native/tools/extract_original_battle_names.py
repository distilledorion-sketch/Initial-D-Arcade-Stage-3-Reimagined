#!/usr/bin/env python3
"""Lossless original battle font export; name codes retain the game's mapping."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import texture_bank

CANONICAL='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image:Path,source:Path,output:Path):
    program=image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=CANONICAL:raise ValueError('Unknown source image')
    addresses=[struct.unpack_from('<I',program,0x33B684-0x20000+i*4)[0] for i in range(31)]
    addresses.append(struct.unpack_from('<I',program,0x33B88C-0x20000)[0])
    names=[]
    for address in addresses:
        begin=address-0x0C020000;name=program[begin:program.index(b'\0',begin)]
        if len(name)>=16 or len(name)%2:raise ValueError('Unexpected original name length')
        names.append(name)
    output.mkdir(parents=True,exist_ok=True)
    indices=(source/'gamekana_font.bin.nz').read_bytes()
    if len(indices)!=9216*2:raise ValueError('Unexpected original font table')
    (output/'font_indices.bin').write_bytes(indices)
    (output/'names.idasname').write_bytes(b'IDAS3N1\0'+struct.pack('<II',1,32)+b''.join(n.ljust(16,b'\0')for n in names))
    bank=texture_bank.export_bank(source/'gamekana_spr.tbl',source/'gamekana_spr.bin.nz',output/'textures','twiddled')
    records=[]
    for i,name in enumerate(names):
        glyphs=[struct.unpack_from('<H',indices,2*((name[j]-160)%256*96+(name[j+1]-160)%256))[0] for j in range(0,len(name),2)]
        if any(g!=0xffff and g>=len(bank['textures'])for g in glyphs):raise ValueError('Original battle name has invalid glyph')
        records.append({'enemy':i if i<31 else None,'source_address':f'{addresses[i]:08X}','name_bytes':name.hex(),'glyphs':glyphs})
    manifest={'schema':'idas3-original-battle-names-v1','source_image_sha256':CANONICAL,'font_table_sha256':hashlib.sha256(indices).hexdigest(),
        'source_rules':'191900 table33B684[enemy]; default player1918C0 pointer33B88C; profile2 forcesenemy30. 68E0 maps source double-byte codes using96-column table.',
        'private_original_assets':True,'names':records}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--source',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();r=export(a.image,a.source,a.out);print(f'Exported {len(r["names"])} exact original names and336 original glyph images')
