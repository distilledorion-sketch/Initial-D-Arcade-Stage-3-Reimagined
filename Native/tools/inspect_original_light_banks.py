"""Inventory original effect geometry and literal references; no runtime launch."""
import argparse,hashlib,json,re,struct
from pathlib import Path
from extract_original_models import parse_model
from export_original_assembly import IMAGE_HASH

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('image','hostfs','output'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();raw=a.image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical original image mismatch')
    banks=[]
    for name in ('lightobj','hilight','effect/flare','effect/taillight'):
        _,chunks,files,payload=parse_model(a.hostfs/'model'/name)
        references=[];seen=set()
        for match in re.finditer(re.escape(name.encode()),raw):
            start=raw.rfind(b'\0',0,match.start())+1;end=raw.find(b'\0',match.start())
            if start in seen:continue
            seen.add(start);address=0x0c020000+start
            references.append(dict(address=f'{address:08X}',text=raw[start:end].decode('ascii'),word_references=[f'{0x0c020000+m.start():08X}' for m in re.finditer(re.escape(struct.pack('<I',address)),raw)]))
        geometry=[]
        for c in chunks:
            vs=[v for batch in c['batches'] for v in batch['vertices']]
            geometry.append(dict(index=c['index'],vertices=len(vs),triangles=sum(len(b['indices'])//3 for b in c['batches']),bounds=[[min(v[k] for v in vs),max(v[k] for v in vs)] for k in (1,2,3)] if vs else None))
        banks.append(dict(bank=name,source_files={k:dict(path=str(f.resolve()),sha256=hashlib.sha256(f.read_bytes()).hexdigest()) for k,f in files.items() if f.exists()},decoded_sha256=hashlib.sha256(payload).hexdigest(),strings=references,chunks=geometry))
    result=dict(source_image_sha256=IMAGE_HASH,banks=banks,scope='Unmodified decoded geometry and literal word references only. This does not establish runtime visibility, scene placement or lighting equations.')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(f'Inventoried {len(banks)} original effect banks; {sum(len(b["chunks"]) for b in banks)} chunks.')
if __name__=='__main__':main()
