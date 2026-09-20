"""Copy all23 authored part transforms per car for native assembly selection."""
import argparse,hashlib,json,math,re,struct
from pathlib import Path
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True);p.add_argument('--hostfs',type=Path,required=True);a=p.parse_args()
    folders=re.findall(r'"([a-z0-9_]+)"',(a.root/'src/car_catalog.h').read_text())
    if len(folders)!=35:raise ValueError('Car catalog')
    rows=[]
    for car,folder in enumerate(folders):
        source=a.hostfs/'parts'/(folder+'.bin');data=source.read_bytes()
        if len(data)!=828 or not all(math.isfinite(x) for x in struct.unpack('<207f',data)):raise ValueError('Original part transforms')
        target=a.root/'data/original_models'/folder/'assembly_parts.bin';target.write_bytes(data)
        rows.append(dict(car=car,folder=folder,sha256=hashlib.sha256(data).hexdigest()))
    (a.root/'data/original_models/assembly_parts_manifest.json').write_text(json.dumps(dict(schema='idas3-original-assembly-parts-v1',cars=rows),indent=2)+'\n')
    print('Preserved805 authored part transforms across35 cars, with no coordinate changes')
if __name__=='__main__':main()
