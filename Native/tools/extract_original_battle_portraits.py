#!/usr/bin/env python3
"""Export exact face-bank identities from original0C9F20/2FBB24."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import extract_original_models as models
import texture_bank

CANONICAL='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image: Path, source: Path, output: Path) -> dict:
    program=image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=CANONICAL:
        raise ValueError('Portrait selectors require the verified canonical image')
    names=[]
    for enemy in range(31):
        address=struct.unpack_from('<I',program,0x2FBB24-0x020000+4*enemy)[0]-0x0C020000
        name=program[address:program.index(b'\0',address)].decode('ascii')
        if not name.startswith('face_') or not name.replace('_','').isalnum():
            raise ValueError('Invalid original portrait filename')
        names.append(name)
    records=[]
    for name in dict.fromkeys(names):
        prefix,chunks,sources,raw=models.parse_model(source,name+'_pol.tbl')
        if len(chunks)!=1:raise ValueError('Unexpected portrait geometry count')
        directory=output/name;directory.mkdir(parents=True,exist_ok=True)
        mesh=directory/(name+'.idasmesh');models.write_binary(mesh,chunks)
        payload=source/(name+'_tex.bin.nz')
        if not payload.exists():payload=source/(name+'_tex.bin')
        decoded=models.read_payload(payload)
        if decoded!=payload.read_bytes():
            payload=directory/'original_texture_decoded.bin';payload.write_bytes(decoded)
        texture_bank.export_bank(source/(name+'_tex.tbl'),payload,directory/'textures','twiddled')
        records.append({'bank':name,'enemy_ids':[i for i,v in enumerate(names)if v==name],
            'mesh':str(mesh.relative_to(output)),'texture_pack':str((directory/'textures/textures.idastex').relative_to(output)),
            'source_polygon_sha256':hashlib.sha256(raw).hexdigest(),
            'source_files':{key:{'path':str(path.resolve()),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}for key,path in sources.items()if path.exists()},
            'chunk_count':len(chunks),'coordinate_transform':'none','uv_transform':'none'})
    result={'schema':'idas3-original-battle-portraits-v1','canonical_image_sha256':CANONICAL,
        'selector':'0C0C9F20 reads2FBB24[enemy]; profile2 forces2FBB9C=face_bunt',
        'private_original_assets':True,'enemy_to_bank':names,'banks':records}
    (output/'manifest.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    return result

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,required=True);parser.add_argument('--source',type=Path,required=True);parser.add_argument('--out',type=Path,required=True)
    args=parser.parse_args();result=export(args.image,args.source,args.out)
    print(f'Exported {len(result["banks"])} original portraits for31 rival identities')
