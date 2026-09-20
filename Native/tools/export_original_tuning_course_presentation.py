"""Offline source-authored tuning-course screen banks and lookup tables."""
from pathlib import Path
import argparse,hashlib,json,struct
from extract_original_menus import export_model_menu,ORIGINAL_IMAGE_SHA256

def export(image,hostfs,root):
    raw=image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=ORIGINAL_IMAGE_SHA256:raise ValueError('Canonical image required')
    base=root/'data/original_assets/tuning_course';base.mkdir(parents=True,exist_ok=True)
    banks=[]
    for bank,name in [(3,'v3sK00rivalface'),(15,'v3sS07tune')]:
        for table,suffix in [(0x0c2a9648,'pol'),(0x0c2a9c78,'tex')]:
            ptr=struct.unpack_from('<I',raw,table-0x0c020000+bank*4)[0]
            if raw[ptr-0x0c020000:].split(b'\0')[0]!=f'/driveA/model/{name}/{name}_{suffix}'.encode():raise ValueError('Resource bank changed')
        result=export_model_menu(hostfs/'model'/name,base/name,'twiddled')
        banks.append(dict(bank=bank,name=name,chunks=result['chunk_count'],textures=result['texture_count']))
    tables=[(0x0c33fc7c,25),(0x0c2a6e14,42),(0x0c2a6ebc,31),(0x0c2a6f38,31),(0x0c26b790,35),(0x0c2a6fb4,25)]
    data=b''.join(raw[a-0x0c020000:a-0x0c020000+n*4]for a,n in tables)
    (base/'tables.bin').write_bytes(data)
    manifest=dict(schema='original-tuning-course-presentation-v1',owner='0C129240',draw='0C12BA00',children=['0C1AF680','0C1B0760'],source_image_sha256=ORIGINAL_IMAGE_SHA256,banks=banks,tables=[dict(address=f'{a:08X}',words=n)for a,n in tables],tables_sha256=hashlib.sha256(data).hexdigest(),private_original_assets=True)
    (base/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
    return manifest
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('root',type=Path);a=p.parse_args();print(json.dumps(export(a.image,a.hostfs,a.root),indent=2))
