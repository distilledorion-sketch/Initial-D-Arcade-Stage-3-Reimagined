#!/usr/bin/env python3
"""Copy exact source course+44 float streams; no synthesized shade samples."""
import argparse,hashlib,json,math,struct
from pathlib import Path
EXPECTED='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
NAMES=('k_ez','s_nm','h_hd','k_df','s_uh','n_sy','k_tu')
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('image','hostfs','project'):p.add_argument('--'+n,type=Path,required=True)
    a=p.parse_args();image=a.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=EXPECTED:raise ValueError('Canonical source hash mismatch')
    out=a.project/'data/original_course_lighting/gain';out.mkdir(parents=True,exist_ok=True)
    records=[]
    for name in NAMES:
        source=a.hostfs/'path'/(name+'_sdw.bin');b=source.read_bytes()
        path=(a.hostfs/'path'/(name+'_path.bin')).read_bytes();count,components=struct.unpack_from('<II',path)
        if components!=3 or len(b)!=count*4:raise ValueError('Source shade/path count mismatch')
        values=struct.unpack('<'+str(count)+'f',b)
        if not all(math.isfinite(v) and 0<=v<=1 for v in values):raise ValueError('Source coefficient range')
        authored=('/driveA/path/'+name+'_sdw.bin').encode()+b'\0'
        if authored not in image:raise ValueError('Missing original resource literal')
        (out/source.name).write_bytes(b)
        records.append(dict(file=source.name,source=str(source.resolve()),bytes=len(b),count=count,minimum=min(values),maximum=max(values),sha256=hashlib.sha256(b).hexdigest()))
    (out/'manifest.json').write_text(json.dumps(dict(format='original raw little-endian float32',source_sha256=EXPECTED,source_reader='0C03D100',records=records),indent=2)+'\n')
    print('Imported7 original car-light streams; total',sum(x['bytes'] for x in records),'bytes')
if __name__=='__main__':main()
