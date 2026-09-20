"""Restore all212 original semantic slots and capture four lighting states."""
import argparse,concurrent.futures,hashlib,json,shutil,struct,subprocess
from pathlib import Path
from export_original_car_colors import pack_geometry
from export_original_assembly import IMAGE_HASH,operations_matrix
from export_car_presentation import CHANNELS

POPUP_CALLS={0x0c027372,0x0c027430}
def key(draw):return json.dumps(draw,sort_keys=True,separators=(',',':'))

def pack_lighting(output,captures):
    base=captures[0]['draws'];lookup={key(d):i for i,d in enumerate(base)}
    if len(lookup)!=len(base):raise ValueError('Ambiguous base draw identity')
    rear=[i for i,d in enumerate(base) if d['slot']==1]
    popup=[i for i,d in enumerate(base) if d['return_pc'] in POPUP_CALLS]
    if len(rear)!=1 or len(popup)>1:raise ValueError('Rear/popup draw identity')
    records=[]
    with (output/'lighting.bin').open('wb') as file:
        file.write(b'ID3LIT1\0'+struct.pack('<II',1,4))
        for state,capture in enumerate(captures):
            brake=bool(state&1);night=bool(state&2);extras=[];order=[]
            for d in capture['draws']:
                if d['slot'] in (1,3):index=rear[0]
                elif d['slot'] in (2,4):index=len(base)
                elif d['slot']==79:index=len(base)+int(brake)
                elif d['return_pc'] in POPUP_CALLS:
                    if not popup:raise ValueError('Unexpected popup geometry')
                    index=popup[0]
                elif key(d) in lookup:index=lookup[key(d)]
                else:
                    if any(op['source'] in CHANNELS for op in d['operations']):raise ValueError('Unbound animated lighting addition')
                    index=len(base)+int(brake)+int(night)+len(extras);extras.append(d)
                order.append(index)
            total=len(base)+int(brake)+int(night)+len(extras)
            if sorted(order)!=list(range(total)):raise ValueError(f'Lighting state{state} is not a complete source draw permutation')
            file.write(struct.pack('<I',len(extras)))
            for d in extras:file.write(struct.pack('<I16f',d['chunk'],*operations_matrix(d['operations'])))
            file.write(struct.pack('<I',len(order))+struct.pack('<'+'I'*len(order),*order))
            records.append(dict(state=state,draws=len(order),extras=[dict(slot=d['slot'],chunk=d['chunk'],return_pc=d['return_pc']) for d in extras]))
    return records

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','image','hostfs','capture-exe'):parser.add_argument('--'+name,type=Path,required=True)
    a=parser.parse_args();program=a.image.read_bytes()
    if hashlib.sha256(program).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical image mismatch')
    cars=json.loads((a.root/'data/original_models/car_catalog.json').read_text())['cars']
    colors=json.loads((a.root/'data/original_models/car_colors_manifest.json').read_text())['cars']
    jobs=[(car['car'],-1,color) for car in colors for color in range(car['count'])]
    jobs += [(struct.unpack_from('<I',program,0x31d8a8-0x20000+enemy*32+8)[0],enemy,0) for enemy in range(31)]
    def export(job):
        car,enemy,color=job;folder=cars[car]['folder'];base=a.root/'data/original_models'/folder
        old=base/'colors'/f'color_{color:02}' if enemy<0 else a.root/'data/original_models/rivals'/f'enemy_{enemy:02}'
        output=base/'appearance_v2'/f'color_{color:02}' if enemy<0 else a.root/'data/original_models/rivals_v2'/f'enemy_{enemy:02}'
        output.mkdir(parents=True,exist_ok=True)
        count=json.loads((base/'model_manifest.json').read_text())['chunk_count']
        partPath=a.hostfs/'parts'/f'{folder}.bin';captures=[]
        for state in range(4):
            source=output/f'state_{state}.json'
            run=subprocess.run([str(a.capture_exe),str(a.image),str(partPath),str(source),str(car),str(count),str(enemy),str(int(bool(state&2))),str(color),str(int(bool(state&1)))],capture_output=True,text=True)
            if run.returncode:raise RuntimeError(run.stdout+run.stderr)
            captures.append(json.loads(source.read_text()))
        for state,capture in enumerate(captures):
            with (output/f'state_{state}.idasasm').open('wb') as file:
                file.write(b'IDAS3A1\0'+struct.pack('<II',1,len(capture['draws'])))
                for d in capture['draws']:file.write(struct.pack('<I16f',d['chunk'],*operations_matrix(d['operations'])))
        phase=struct.unpack_from('<I',program,0x298320-0x20000+car*4)[0]
        pack_geometry(output,captures[0],captures[2],partPath.read_bytes(),phase)
        states=pack_lighting(output,captures)
        shutil.copyfile(old/'materials.bin',output/'materials.bin')
        oldDraws=json.loads((old/'source_capture.json').read_text())['draws']
        retained=[d for d in captures[0]['draws'] if d['semantic']<187]
        if retained!=oldDraws:raise ValueError('Restoration unexpectedly changed pre-existing day draws')
        (output/'source_capture.json').write_text(json.dumps(captures[0]))
        (output/'night_capture.json').write_text(json.dumps(captures[2]))
        record=dict(car=car,enemy=enemy,color=color,day_draws=len(captures[0]['draws']),old_day_draws=len(oldDraws),restored_day_slots=[d['semantic'] for d in captures[0]['draws'] if d['semantic']>=187],lighting_states=states)
        (output/'manifest.json').write_text(json.dumps(record,indent=2)+'\n')
        return record
    with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool:
        records=list(pool.map(export,jobs))
    manifest=dict(schema='idas3-car-geometry-v2',source_image_sha256=IMAGE_HASH,appearances=records,
        source='026100 slots0..211, 026D80 drawing, four day/night and brake states; original matrix-helper arguments captured at explicit boundaries. Prior materials and plate transforms preserved. No hardware pixel-equivalence claim.')
    (a.root/'data/original_models/car_geometry_v2_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Captured{len(records)}appearances;{sum(r["day_draws"]-r["old_day_draws"] for r in records)}restored day draws;{sum(len(s["extras"]) for r in records for s in r["lighting_states"])}lighting additions across four states.')

if __name__=='__main__':main()
