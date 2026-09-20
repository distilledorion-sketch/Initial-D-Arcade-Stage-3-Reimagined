#!/usr/bin/env python3
"""Add live original wheel programs without changing existing model/assembly assets."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
from export_original_assembly import operations_matrix, IMAGE_HASH

KINDS = {'translate':0, 'scale':1, 'load_matrix':2, 'rotate_x_u16':3,
         'rotate_y_u16':4, 'rotate_z_u16':5, 'rotate_x_float':6,
         'rotate_y_float':7, 'rotate_z_float':8}
WHEEL_CALLS = (0x0c027506,0x0c02765c,0x0c02773c,0x0c02785c)
# channel0=static;1=steer;2..5=suspension;6..9=spin. Negative means FNEG.
CHANNELS = {0x0c027482:1,0x0c02756a:1,0x0c0278fa:1,
            0x0c0274dc:2,0x0c027630:3,0x0c027712:4,0x0c0277da:5,
            0x0c0274e4:6,0x0c02763a:-7,0x0c02771a:8,0x0c0277e4:-9}

def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--root',type=Path,required=True)
    ap.add_argument('--image',type=Path,required=True)
    ap.add_argument('--hostfs',type=Path,required=True)
    ap.add_argument('--capture-exe',type=Path,required=True)
    a=ap.parse_args()
    if hashlib.sha256(a.image.read_bytes()).hexdigest()!=IMAGE_HASH:
        raise ValueError('Canonical source image mismatch')
    cars=json.loads((a.root/'data/original_models/car_catalog.json').read_text())['cars']
    summary=[]
    for car in cars:
        ci,folder=car['car_index'],car['folder']
        base=a.root/'data/original_models'/folder
        out=base/'presentation';out.mkdir(exist_ok=True)
        bank=json.loads((base/'model_manifest.json').read_text())
        source=base/'assembly/source_capture.json'
        if ci in (6,29,33,34):
            source=out/'stock_wheel_capture.json'
            subprocess.run([str(a.capture_exe),str(a.image),str(a.hostfs/'parts'/f'{folder}.bin'),str(source),str(ci),str(bank['chunk_count'])],check=True)
        capture=json.loads(source.read_text())
        programs=[]; wheels=[]
        with (out/f'{folder}_playable.idasasm').open('wb') as f:
            f.write(b'IDAS3A1\0'+struct.pack('<II',1,len(capture['draws'])))
            for index,draw in enumerate(capture['draws']):
                f.write(struct.pack('<I16f',draw['chunk'],*operations_matrix(draw['operations'])))
                if draw['return_pc'] in WHEEL_CALLS:
                    wheels.append({'instance':index,'chunk':draw['chunk'],'semantic':draw['semantic'],'wheel':WHEEL_CALLS.index(draw['return_pc'])})
                ops=[];dynamic=False
                for op in draw['operations']:
                    channel=CHANNELS.get(op['source'],0)
                    if op['source']==0x0c02795e:
                        channel=2+(1,0,3,2)[draw['semantic']-60]
                    dynamic|=channel!=0
                    values=op['values']+[0.0]*(16-len(op['values']))
                    ops.append(struct.pack('<Ii16f',KINDS[op['type']],channel,*values))
                if dynamic:programs.append(struct.pack('<II',index,len(ops))+b''.join(ops))
        if len(wheels)!=4 or {w['wheel'] for w in wheels}!={0,1,2,3}:
            raise ValueError(f'{folder}: original capture does not contain four wheel draws')
        (out/'wheel_motion.bin').write_bytes(b'ID3MOT1\0'+struct.pack('<II',1,len(programs))+b''.join(programs))
        summary.append({'car':ci,'folder':folder,'stock_wheel_repair':ci in (6,29,33,34),'wheels':wheels,'animated_instances':len(programs),'capture_sha256':hashlib.sha256(source.read_bytes()).hexdigest()})
    manifest={'schema':'idas3-car-presentation-v1','source_image_sha256':IMAGE_HASH,
              'wheel_selection':'Four absent display-preset variants replaced by original 0285C0 stock input0; actual player invokes this setter from tuning byte7 at06311E.',
              'actor_binding':'034840 copies actor40..4C suspension,60..6C spin,3C steering into ACar2F4..314. 026D80 applies captured operations; left spins negated at027636/0277E0.',
              'scope':'Existing body/display configuration preserved. Wheel blur and other tuning choices remain outside this addition.',
              'cars':summary}
    (a.root/'data/original_models/car_presentation_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Exported {len(cars)} car presentations, 140 original wheel instances; repaired four absent display variants.')

if __name__=='__main__':main()
