"""Read-only material/byte reference inventory; never starts the original runtime."""
from pathlib import Path
import argparse, collections, hashlib, json, struct

def inspect_model(path):
    counts=collections.Counter(); vertices=collections.Counter(); bump=0; records=[]
    with path.open('rb') as f:
        def words(n):
            b=f.read(n*4)
            if len(b)!=n*4: raise ValueError('truncated '+str(path))
            return struct.unpack('<'+'I'*n,b)
        if f.read(8)!=b'IDAS3M1\0': raise ValueError('bad model signature')
        version,nc=words(2)
        if version!=1: raise ValueError('bad version')
        for ci in range(nc):
            ix,source,size,nb=words(4); words(24)
            if ix!=ci: raise ValueError('chunk order')
            for bi in range(nb):
                source,nv,ni,raw=words(4); ich=words(8); material=words(16)
                mode=(ich[2]>>22)&3; counts[mode]+=1; vertices[mode]+=nv
                # ICH TCW is word3. Bump pixel format is4, TCW27..29.
                is_bump=((ich[3]>>27)&7)==4
                bump+=is_bump
                if mode in (1,3) or is_bump:
                    records.append(dict(chunk=ci,batch=bi,source=hex(source),fogMode=mode,pcw=hex(ich[0]),tsp=hex(ich[2]),tcw=hex(ich[3]),bump=is_bump))
                f.seek(nv*44+ni*4+raw,1)
        if f.read(1): raise ValueError('trailing model bytes')
    return dict(path=str(path),batchModes=dict(sorted(counts.items())),vertexModes=dict(sorted(vertices.items())),bumpBatches=bump,special=records)

def inspect_material_patch(path):
    data=path.read_bytes()
    if data[:8]!=b'ID3CMP1\0':raise ValueError('bad material patch')
    version,count=struct.unpack_from('<II',data,8)
    if version!=1 or len(data)!=16+count*200:raise ValueError('bad patch size/version')
    counts=collections.Counter();bump=0;records=[]
    for i in range(count):
        chunk,batch=struct.unpack_from('<II',data,16+i*200)
        after=struct.unpack_from('<24I',data,16+i*200+8+96)
        mode=(after[18]>>22)&3;counts[mode]+=1;is_bump=((after[19]>>27)&7)==4;bump+=is_bump
        if mode in (1,3) or is_bump:records.append(dict(chunk=chunk,batch=batch,fogMode=mode,bump=is_bump))
    return dict(path=str(path),recordCount=count,batchModes=dict(sorted(counts.items())),bumpBatches=bump,special=records)

def main():
    parser=argparse.ArgumentParser();parser.add_argument('project',type=Path);parser.add_argument('image',type=Path);parser.add_argument('output',type=Path)
    a=parser.parse_args();data=a.image.read_bytes();base=0x0c020000
    sha=hashlib.sha256(data).hexdigest()
    if sha!='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335':raise ValueError('noncanonical image')
    targets=(0x0c1fac20,0x0c1f8240,0x0c2122c0,0xa05f80b4)
    refs={hex(t):{'literalWords':[],'pcRelativeLoads':[],'directBranches':[]} for t in targets}
    for off in range(0,len(data)-4,2):
        address=base+off;op=struct.unpack_from('<H',data,off)[0]
        if off%4==0:
            value=struct.unpack_from('<I',data,off)[0]
            if value in targets:refs[hex(value)]['literalWords'].append(hex(address))
        if op>>12==13:
            literal=((address+4)&~3)+(op&255)*4
            if base<=literal<=base+len(data)-4:
                value=struct.unpack_from('<I',data,literal-base)[0]
                if value in targets:refs[hex(value)]['pcRelativeLoads'].append(hex(address))
        if op>>12 in (10,11):
            displacement=op&4095
            if displacement&2048:displacement-=4096
            target=address+4+displacement*2
            if target in targets:refs[hex(target)]['directBranches'].append(hex(address))
    model_root=a.project/'data/original_models'
    folders=[p for p in model_root.iterdir() if p.is_dir() and (p.name in ('courses','rivals','rivals_v2','numberplate') or p.name.startswith(('honda_','mazda_','mitsu_','nissan_','subaru_','suzuki_','toyota_')))]
    models=[]
    for folder in sorted(folders):
        for p in sorted(folder.rglob('*.idasmesh')):
            item=inspect_model(p);item['group']='courses' if folder.name=='courses' else 'carsAndPlates';models.append(item)
    if not models:raise ValueError('no course models found')
    total=collections.Counter();vtotal=collections.Counter()
    for m in models:total.update(m['batchModes']);vtotal.update(m['vertexModes'])
    groups={}
    for group in ('courses','carsAndPlates'):
        subset=[m for m in models if m['group']==group];c=collections.Counter();v=collections.Counter()
        for m in subset:c.update(m['batchModes']);v.update(m['vertexModes'])
        groups[group]=dict(modelCount=len(subset),batchModes=dict(sorted(c.items())),vertexModes=dict(sorted(v.items())),bumpBatches=sum(m['bumpBatches'] for m in subset))
    patches=[inspect_material_patch(p) for folder in sorted(folders) if folder.name!='courses' for p in sorted(folder.rglob('materials.bin'))]
    patch_counts=collections.Counter()
    for p in patches:patch_counts.update(p['batchModes'])
    patch_summary=dict(fileCount=len(patches),recordCount=sum(p['recordCount'] for p in patches),batchModes=dict(sorted(patch_counts.items())),bumpBatches=sum(p['bumpBatches'] for p in patches))
    result=dict(imageSha256=sha,staticReferenceScope='Aligned literal words, all even PC-relative loads, and direct BRA/BSR encodings; data can resemble code. Indirect dynamic targets and arbitrary register writes are not excluded.',vertexFogSetterReferences=refs,modelCount=len(models),batchModes=dict(sorted(total.items())),vertexModes=dict(sorted(vtotal.items())),bumpBatches=sum(m['bumpBatches'] for m in models),groups=groups,patchSummary=patch_summary,models=models,patches=patches)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('models','patches')},indent=2))
if __name__=='__main__':main()
