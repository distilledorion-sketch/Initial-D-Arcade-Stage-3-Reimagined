"""Capture separate original fresh-player and31 authored enemy appearances."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from export_car_presentation import KINDS,CHANNELS,WHEEL_CALLS
from export_original_assembly import operations_matrix,IMAGE_HASH

def export(root,host,image,exe,car,enemy):
    ci,folder=car['car_index'],car['folder'];base=root/'data/original_models'/folder
    target=base/'fresh_player' if enemy<0 else root/'data/original_models/rivals'/f'enemy_{enemy:02}'
    target.mkdir(parents=True,exist_ok=True);source=target/'source_capture.json'
    count=json.loads((base/'model_manifest.json').read_text())['chunk_count']
    subprocess.run([str(exe),str(image),str(host/'parts'/f'{folder}.bin'),str(source),str(ci),str(count),str(enemy)],check=True,capture_output=True)
    capture=json.loads(source.read_text());programs=[];wheels=[]
    with (target/'car.idasasm').open('wb') as out:
        out.write(b'IDAS3A1\0'+struct.pack('<II',1,len(capture['draws'])))
        for index,draw in enumerate(capture['draws']):
            out.write(struct.pack('<I16f',draw['chunk'],*operations_matrix(draw['operations'])))
            if draw['return_pc'] in WHEEL_CALLS:wheels.append(dict(instance=index,chunk=draw['chunk'],semantic=draw['semantic'],wheel=WHEEL_CALLS.index(draw['return_pc'])))
            ops=[];dynamic=False
            for op in draw['operations']:
                channel=CHANNELS.get(op['source'],0)
                if op['source']==0x0c02795e:channel=2+(1,0,3,2)[draw['semantic']-60]
                dynamic|=channel!=0
                ops.append(struct.pack('<Ii16f',KINDS[op['type']],channel,*(op['values']+[0.]*(16-len(op['values'])))))
            if dynamic:programs.append(struct.pack('<II',index,len(ops))+b''.join(ops))
    if len(wheels)!=4 or {w['wheel'] for w in wheels}!={0,1,2,3}:raise ValueError(f'{folder}/enemy{enemy}: missing original four wheel selections: {wheels}')
    (target/'wheel_motion.bin').write_bytes(b'ID3MOT1\0'+struct.pack('<II',1,len(programs))+b''.join(programs))
    config=capture['config_word'];parts=(host/'parts'/f'{folder}.bin').read_bytes()
    offsets=((config&7)*36,0xd8+((config>>13)&7)*36)
    transforms=[struct.unpack_from('<9f',parts,n) for n in offsets]
    (target/'plate.bin').write_bytes(b'ID3PLT1\0'+struct.pack('<II',1,1)+struct.pack('<I18f',config,*transforms[0],*transforms[1]))
    return dict(car=ci,folder=folder,enemy=enemy,config=config,plate_digits=capture['plate_digits'],plate_part_offsets=offsets,wheels=wheels,
        draws=len(capture['draws']),animated_instances=len(programs),capture_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),parts_sha256=hashlib.sha256(parts).hexdigest())

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','image','hostfs','capture-exe'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();program=a.image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical program mismatch')
    cars=json.loads((a.root/'data/original_models/car_catalog.json').read_text())['cars'];players=[];enemies=[]
    for car in cars:
        players.append(export(a.root,a.hostfs,a.image,a.capture_exe,car,-1))
        print('Fresh player',car['car_index'],car['folder'],players[-1]['draws'],flush=True)
    for enemy in range(31):
        ci=struct.unpack_from('<I',program,0x31d8a8-0x020000+enemy*32+8)[0]
        enemies.append(export(a.root,a.hostfs,a.image,a.capture_exe,cars[ci],enemy))
        print('Rival',enemy,cars[ci]['folder'],enemies[-1]['draws'],flush=True)
    manifest=dict(schema='idas3-original-car-appearances-v1',source_image_sha256=IMAGE_HASH,
        source='Fresh profile134A60; constructor228DC0/026436..0264BC; player0630B4..06316E versus rival035F00(enemy); semantic visibility029040..02988E; draw026D80.',
        boundaries='Unsigned division2223B8 is a bounded arithmetic hook. Graphics/matrix call arguments captured, matrices reconstructed.02988E onward material alpha/paint and dynamic popup-headlight interpolation remain separate; no hardware rendering parity claimed.',
        players=players,rivals=enemies)
    (a.root/'data/original_models/car_appearances_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    text='#pragma once\n#include <array>\n#include <cstdint>\nnamespace idas3 {\n// Exact31D8A8 car IDs and035F00/2F48CC number-plate digits.\nstruct OriginalRivalAppearance {unsigned car;std::array<std::uint8_t,5> digits;};\ninline constexpr std::array<OriginalRivalAppearance,31> originalRivalAppearances{{\n'
    text+='\n'.join('    {'+str(r['car'])+', {'+','.join(map(str,r['plate_digits']))+'}}, // enemy'+str(r['enemy']) for r in enemies)
    text+='\n}};\n}\n';(a.root/'src/original_rival_appearance_catalog.h').write_text(text)
    print('Captured35 fresh players and31 exact enemy presets,264 selected wheels.')
if __name__=='__main__':main()
