#!/usr/bin/env python3
"""Import original Demo7 animation records; does not synthesize a video/camera."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_models import read_payload

CANONICAL='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path);a=p.parse_args()
    image=a.image.read_bytes();assert hashlib.sha256(image).hexdigest()==CANONICAL
    def words(at,n):return struct.unpack_from('<'+'I'*n,image,at-0xc020000)
    root=a.project/'data/original_assets/attract/demo';root.mkdir(parents=True,exist_ok=True)
    evidence=a.project/'verification/original-demo';evidence.mkdir(parents=True,exist_ok=True)
    sources={};banks={}
    for src,dst in [('o_advcg.bin.nz','actors.bin'),('o_advcg.tbl','shots.bin'),('o_camadv.bin.nz','cameras.bin')]:
        path=a.hostfs/'binary'/src;b=read_payload(path);(root/dst).write_bytes(b);banks[dst]=b
        sources[src]={'bytes':len(b),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'decoded_sha256':hashlib.sha256(b).hexdigest()}
    raw=banks['shots.bin'];poses=banks['actors.bin'];cameras=banks['cameras.bin'];count=len(raw)//24
    assert len(raw)%24==0 and len(poses)%336==0 and struct.unpack_from('<I',cameras)[0]==count
    # Original0E2240 constructs ACar from configuration+52, then0E1E00
    # applies035F00 enemy appearance from configuration+24. These are distinct.
    car_ids=words(0xc2624d4,6);enemy_ids=words(0xc2624b8,6);pairs=words(words(0xc3129c4,1)[0],count*2)
    bindings=[car_ids[x] for x in pairs];(root/'cars.bin').write_bytes(struct.pack('<'+'I'*len(bindings),*bindings))
    enemies=[enemy_ids[x] for x in pairs];(root/'enemies.bin').write_bytes(struct.pack('<'+'I'*len(enemies),*enemies))
    appearances=[]
    for car,enemy in zip(car_ids,enemy_ids):
        capture=json.loads((a.project/'data/original_models/rivals_v2'/f'enemy_{enemy:02}'/'source_capture.json').read_text())
        if capture['car_index']!=car or capture['enemy_id']!=enemy:raise ValueError('Rival appearance identity mismatch')
        appearances.append({'car':car,'enemy':enemy,'config_word':capture['config_word'],'material_variant':7,'plate_digits':capture['plate_digits']})
    (root/'appearances.bin').write_bytes(b''.join(struct.pack('<4I5B3x',r['car'],r['enemy'],r['config_word'],7,*r['plate_digits']) for r in appearances))
    (evidence/'appearances.json').write_text(json.dumps({'source':'0E2240 constructor car table2624D4;0E1E00 enemy table2624B8 through035F00;0E2076 sets0286A0(7); existing isolated source rival capture provides final config word/digits.','records':appearances},indent=2)+'\n')
    cumulative=0;cursor=4;shots=[]
    for i in range(count):
        raw_frames=struct.unpack_from('<6I',raw,i*24);frames=[x+cumulative for x in raw_frames[:4]]+list(raw_frames[4:]);cumulative+=raw_frames[1]+1
        n=struct.unpack_from('<I',cameras,cursor)[0];assert n%4==0 and n>0 and cursor+4+n<=len(cameras)
        cam=struct.unpack_from('<'+'I'*(n//4),cameras,cursor+4)
        shots.append({'shot':i,'source_table':list(raw_frames),'frames':frames,'camera_offset':cursor+4,'camera_bytes':n,'camera_type':cam[0],'actor_slots':list(pairs[i*2:i*2+2]),'car_ids':bindings[i*2:i*2+2],'enemy_ids':enemies[i*2:i*2+2],'material_variant':7});cursor+=4+n
    assert cumulative==len(poses)//336
    for s in shots:s['descriptor_offset']=cursor;s['descriptor']=list(struct.unpack_from('<11I',cameras,cursor));cursor+=44
    report={'schema':'idas3-original-demo-v1','image_sha256':CANONICAL,'registered_child':7,'constructor':'0C0E5740','init':'0C0E6280','main':'0C0E6FC0','sources':sources,'frame_count':cumulative,'actor_count':2,'actor_bytes':168,'shots':shots,'camera_payload_consumed_bytes':cursor,'camera_padding_bytes':len(cameras)-cursor,'source_car_ids':list(car_ids),'source_scope':'Raw actor records, exact shot relocation0E64CC and original157580/157640 copy/advance. Camera types/parameters and descriptor words preserved. Native camera evaluation and whole-owner presentation are separate integration requirements.'}
    report['source_enemy_ids']=list(enemy_ids)
    report['presentation_events']={'body_submission':'both selected actors every frame','headlights_on':500,'headlights_off_actor1':5550,'headlights_off_actor0':5720,'light_effects_on':535,'finish_after_old_timeline':5830}
    report['path_seed']='At each shot reset, descriptor words7/9 become actor0/1 original authoring-path indices; fractions are explicitly zeroed. Each actor update projects raw XYZ through original096200 before selected actor0 index feeds03A2C0/03FCE0 scene selection.'
    (evidence/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');print(f'Imported {count} original shots, {cumulative} frames and {len(bindings)} exact car bindings')
if __name__=='__main__':main()
