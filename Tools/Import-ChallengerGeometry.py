"""Extract the original interrupt banner mesh without scaling/redrawing it."""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'Native/tools'))
from extract_original_models import parse_model

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--hostfs',type=Path,required=True)
parser.add_argument('--check',action='store_true')
args=parser.parse_args()
directory=args.hostfs/'model/interrupt'
_,chunks,_,_=parse_model(directory)
assert len(chunks)==3 and [len(c['batches']) for c in chunks]==[3,1,1]
parts=[]
for chunk in chunks[:2]:
    for batch in chunk['batches']:
        parts.append(dict(texture=batch['material'][9],vertices=[n for v in batch['vertices'] for n in (*v[1:4],*v[7:9])],triangles=batch['indices']))
assert [p['texture'] for p in parts]==[0,1,2,3]
result=dict(source='HOSTFS/model/interrupt/interrupt02_pol',
    sourceSha256={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in (directory/'interrupt02_pol.tbl',directory/'interrupt02_pol.bin.nz')},
    parts=parts)
output=ROOT/'Assets/Resources/Challenger/geometry.json'
text=json.dumps(result,indent=2)+'\n'
if args.check:
    assert output.read_text()==text,'Challenger geometry differs from original source'
else:
    output.write_text(text)
print('PASS original challenger geometry: 4 batches,',sum(len(p['vertices'])//5 for p in parts),'vertices')
