"""Export source-executed GMP/ICH overrides, retaining exact precondition words."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from extract_original_models import parse_model,VERTEX_SIZE
from export_original_assembly import IMAGE_HASH

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','hostfs','image','capture-exe','work'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();a.work.mkdir(parents=True,exist_ok=True)
    if hashlib.sha256(a.image.read_bytes()).hexdigest()!=IMAGE_HASH:raise ValueError('Original program identity')
    catalog=json.loads((a.root/'data/original_models/car_appearances_manifest.json').read_text())
    summary=[]
    for appearance in catalog['players']+catalog['rivals']:
        car,enemy,folder=appearance['car'],appearance['enemy'],appearance['folder']
        target=a.root/'data/original_models'/folder/'fresh_player' if enemy<0 else a.root/'data/original_models/rivals'/f'enemy_{enemy:02}'
        _,chunks,paths,source=parse_model(a.hostfs/'model/car'/folder)
        raw=a.work/'source.bin';raw.write_bytes(source);changed=a.work/'changed.bin';meta=a.work/'capture.json'
        run=subprocess.run([str(a.capture_exe),str(a.image),str(raw),str(paths['table']),str(car),str(enemy),str(changed),str(meta)],check=True,capture_output=True)
        after=changed.read_bytes();record=json.loads(meta.read_text());patches=[];allowed=set();painted=[]
        if len(after)!=len(source):raise ValueError('Capture changed source extent')
        for chunk in chunks:
            if chunk.get('nongeometry_marker'):continue
            pos=chunk['source_offset']+96;end=chunk['source_offset']+chunk['source_size'];material=None;batch=0
            while pos<end:
                words=struct.unpack_from('<8I',source,pos);cmd=(words[0]>>8)&15
                if cmd==5:material=pos;allowed.update(range(pos,pos+64));pos+=64;continue
                if cmd!=7 or material is None:raise ValueError('Unexpected source command')
                allowed.update(range(pos,pos+32))
                old=struct.unpack_from('<16I',source,material)+struct.unpack_from('<8I',source,pos)
                new=struct.unpack_from('<16I',after,material)+struct.unpack_from('<8I',after,pos)
                if old!=new:
                    patches.append(struct.pack('<II48I',chunk['index'],batch,*old,*new))
                    if old[3]!=new[3]:painted.append(dict(chunk=chunk['index'],batch=batch,before=f'{old[3]:08x}',after=f'{new[3]:08x}'))
                batch+=1;pos+=32+VERTEX_SIZE[words[6]]*words[7]
        unexpected=[n for n,(x,y) in enumerate(zip(source,after)) if x!=y and n not in allowed]
        if unexpected:raise ValueError(f'Writes outside material/ICH records: {unexpected[:8]}')
        packed=b'ID3CMP1\0'+struct.pack('<II',1,len(patches))+b''.join(patches)
        (target/'materials.bin').write_bytes(packed)
        record.update(schema='idas3-original-car-materials-v1',source_image_sha256=IMAGE_HASH,source_polygon_sha256=hashlib.sha256(source).hexdigest(),output_sha256=hashlib.sha256(packed).hexdigest(),batch_overrides=len(patches),diffuse_changes=painted,
            source='0267C0 constructor material setup and masks;029040 through029AD0 including029E60→1911A0 palette→028DA0 RGB,028B80/028C80 alpha,028EC0 specular,029ABC gloss writes. Later suspension lookup excluded.',
            boundaries='Actual1D4BE0/1D4C40 parser. Explicit hooks: diagnostic output, completed texture upload/binding, byte-copy allocation and classification of shadow copies, discarded16D680 course-query return, compiler unsigned division. No host color invention.')
        (target/'materials_manifest.json').write_text(json.dumps(record,indent=2)+'\n');summary.append(record)
        print(f'car{car}/enemy{enemy}: RGB{record["rgb"]}, {len(patches)} batches',flush=True)
    (a.root/'data/original_models/car_materials_manifest.json').write_text(json.dumps(dict(schema='idas3-car-material-catalog-v1',appearances=summary),indent=2)+'\n')
    print(f'Exported {len(summary)} original material appearances')
if __name__=='__main__':main()
