"""Preserve authored GMP sharing and semantic maps for native material rebuilds.

No behavior captures: all records come directly from the model bank and the
canonical program's static slot map. Unused GMPs are retained as parser events.
"""
import argparse, hashlib, json, re, struct
from pathlib import Path
from extract_original_models import read_payload, VERTEX_SIZE
from export_original_assembly import IMAGE_HASH

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','hostfs','image'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args(); image=a.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical image identity')
    folders=re.findall(r'"([a-z0-9_]+)"',(a.root/'src/car_catalog.h').read_text())
    if len(folders)!=35:raise ValueError('Car catalog')
    records=[]
    for car,folder in enumerate(folders):
        directory=a.hostfs/'model/car'/folder
        tables=list(directory.glob('*_pol.tbl'))
        if len(tables)!=1:raise ValueError('Model table count')
        table=tables[0]; polygon=table.with_suffix('.bin.nz')
        if not polygon.exists():polygon=table.with_suffix('.bin')
        raw=read_payload(polygon); offsets=struct.unpack('<'+'I'*(table.stat().st_size//4),table.read_bytes())
        pointer=struct.unpack_from('<I',image,0x0c33b250-0x0c020000+4*car)[0]
        slots=struct.unpack_from('<212i',image,pointer-0x0c020000)
        out=bytearray(b'ID3CML1\0'+struct.pack('<III212i',1,car,len(offsets),*slots))
        total_materials=total_batches=0
        for index,start in enumerate(offsets):
            end=offsets[index+1] if index+1<len(offsets) else len(raw)
            materials=[]; batches=[]; pos=start+96
            if struct.unpack_from('<I',raw,start)[0]==0xffffffff:pos=end
            while pos<end:
                words=struct.unpack_from('<8I',raw,pos); command=(words[0]>>8)&15
                if command==5:
                    materials.append((pos,struct.unpack_from('<16I',raw,pos)));pos+=64
                elif command==7 and materials:
                    batches.append((pos,len(materials)-1,words))
                    pos+=32+VERTEX_SIZE[words[6]]*words[7]
                else:raise ValueError('Unexpected material event')
            if pos!=end:raise ValueError('Material event extent')
            out.extend(struct.pack('<4I',start,end-start,len(materials),len(batches)))
            for off,words in materials:out.extend(struct.pack('<I16I',off,*words))
            for off,group,words in batches:out.extend(struct.pack('<II8I',off,group,*words))
            total_materials+=len(materials);total_batches+=len(batches)
        target=a.root/'data/original_models'/folder/'material_layout.bin';target.write_bytes(out)
        records.append(dict(car=car,folder=folder,materials=total_materials,batches=total_batches,
            polygon_sha256=hashlib.sha256(raw).hexdigest(),layout_sha256=hashlib.sha256(out).hexdigest()))
        print(f'{folder}: {total_materials} authored materials / {total_batches} batches',flush=True)
    (a.root/'data/original_models/material_layout_manifest.json').write_text(json.dumps(dict(
        schema='idas3-car-material-layout-v1',source_image_sha256=IMAGE_HASH,cars=records),indent=2)+'\n')
if __name__=='__main__':main()
