"""Preserve original number-plate geometry, textures, and per-car placement records."""
import argparse, hashlib, json, struct
from pathlib import Path
from extract_original_models import parse_model, write_binary
from texture_bank import export_bank

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--image',type=Path,required=True)
    p.add_argument('--project',type=Path,required=True);a=p.parse_args()
    root=a.project/'data/original_models/numberplate';root.mkdir(parents=True,exist_ok=True)
    banks=[]
    for name in ('numberplate','numberplate_y'):
        source=a.hostfs/'model/numberplate'
        _,chunks,files,raw=parse_model(source,name+'_pol.tbl',name+'_tex.tbl')
        assert len(chunks)==11
        write_binary(root/(name+'.idasmesh'),chunks)
        tex=export_bank(source/(name+'_tex.tbl'),source/(name+'_tex.bin.nz'),a.project/'data/original_assets/numberplate'/name/'textures','twiddled')
        banks.append(dict(name=name,source=str(files['polygon']),source_sha256=hashlib.sha256(raw).hexdigest(),chunks=len(chunks),textures=len(tex['textures'])))
    program=a.image.read_bytes();records=[]
    catalog=json.loads((a.project/'data/original_models/car_catalog.json').read_text())
    # Source catalog schema is explicit: ID order was recovered from2EF410.
    cars=catalog['cars'] if isinstance(catalog,dict) else catalog
    with (root/'placement.bin').open('wb') as out:
        out.write(b'ID3PLT1\0'+struct.pack('<II',1,35))
        for car in cars:
            ci=car.get('id',car.get('car_index',car.get('index')));name=car.get('folder',car.get('name'))
            assert ci==len(records)
            capture=json.loads((a.project/'data/original_models'/name/'assembly/source_capture.json').read_text())
            config=capture['config_word'];parts_path=a.hostfs/'parts'/(name+'.bin');parts=parts_path.read_bytes()
            offsets=((config&7)*36,0xd8+((config>>13)&7)*36)
            transforms=[struct.unpack_from('<9f',parts,offset) for offset in offsets]
            out.write(struct.pack('<I18f',config,*transforms[0],*transforms[1]))
            records.append(dict(car=ci,folder=name,config_word=config,source=str(parts_path),parts_sha256=hashlib.sha256(parts).hexdigest(),offsets=offsets,transforms=transforms))
    manifest=dict(schema='idas3-number-plate-v1',program_sha256=hashlib.sha256(program).hexdigest(),banks=banks,cars=records,
        source_draw='026160: base chunk10 and five digit chunks.026D80 selects front/rear records and applies T*RZ*RY*RX*S.',
        fresh_digits=[2,2,9,3,6],fresh_source='1348A0 clears profile+12.0631BE supplies five zeros to057280; additive constants modulo100000 produce22936.')
    (root/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Exported both original11-chunk plate banks and35 exact placement records')
if __name__=='__main__':main()
