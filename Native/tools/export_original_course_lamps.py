"""Capture original lamp draws without changing existing scene assets."""
import argparse,hashlib,json,struct,subprocess
from pathlib import Path
from export_original_assembly import IMAGE_HASH

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('image','hostfs','project','capture_exe'):p.add_argument('--'+n.replace('_','-'),type=Path,required=True)
    p.add_argument('--courses',default='1,4,5');a=p.parse_args()
    if hashlib.sha256(a.image.read_bytes()).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical image mismatch')
    rules={1:('s_nm',4),4:('s_vh',72),5:('s_uh',189)}
    paths={r['id']:r['points'] for r in json.loads((a.project/'data/courses/manifest.json').read_text())['courses']}
    output=a.project/'verification/course-lamps-0317';output.mkdir(parents=True,exist_ok=True);records=[]
    for course in map(int,a.courses.split(',')):
        cid,chunk=rules[course];base=a.project/'data/original_models/courses'/cid
        lampPath=a.hostfs/'path'/(cid+'_path_lamp.bin');lampBytes=lampPath.read_bytes()
        count,components=struct.unpack_from('<II',lampBytes)
        if components!=3 or len(lampBytes)!=8+12*count:raise ValueError('Lamp path header')
        positions=[list(struct.unpack_from('<3f',lampBytes,8+12*i)) for i in range(count)]
        banks=json.loads((base/'banks_manifest.json').read_text())
        for v in banks['variants']:
            night=v['night'];wet=v.get('weather',0)
            # The day banks of Usui/Irohazaka do not contain their night lamp
            # chunks. Their source constructors/draws do not enable lamps.
            if not night and not wet and course!=4:continue
            for reverse in (0,1):
                name=('night' if night else 'day')+('_reverse' if reverse else '_forward')+('_wet' if wet else '')
                old=json.loads((base/(name+'_capture.json')).read_text());capture=output/(cid+'_'+name+'.json')
                subprocess.run([str(a.capture_exe),str(a.image),str(course*2+night),str(reverse),str(paths[cid]),str(v['world_chunk_count']),str(capture),str(wet),str(lampPath),str(chunk)],check=True)
                new=json.loads(capture.read_text())
                for key in ('assemblies','changes','boundaries','path_count'):
                    if new[key]!=old[key]:raise ValueError(f'{cid} {name}: existing {key} changed')
                enabled=night==1 or course==4
                insertions=[]
                for index,lamps in enumerate(new['lamp_assemblies']):
                    if len(lamps)!=(count if enabled else 0):raise ValueError('Original lamp enable/count changed')
                    if not enabled:insertions.append(0);continue
                    before=lamps[0]['before']
                    for lamp,position in zip(lamps,positions):
                        if lamp['before']!=before or lamp['chunk']!=chunk or struct.pack('<3f',*lamp['position'])!=struct.pack('<3f',*position):raise ValueError('Original lamp order/position/chunk differs')
                    if before>len(old['assemblies'][index]):raise ValueError('Lamp insertion bound')
                    insertions.append(before)
                destination=base/('scene_'+name+'.idaslamps')
                with destination.open('wb') as f:
                    f.write(b'ID3LMP1\0'+struct.pack('<III',chunk,count if enabled else 0,len(insertions)))
                    if enabled:f.write(lampBytes[8:])
                    f.write(struct.pack('<'+'I'*len(insertions),*insertions))
                records.append(dict(course=cid,variant=name,enabled=enabled,chunk=chunk,positions=count if enabled else 0,assemblies=len(insertions),source_path=str(lampPath.resolve()),source_sha256=hashlib.sha256(lampBytes).hexdigest(),original_instructions=new['original_instructions'],capture_sha256=hashlib.sha256(capture.read_bytes()).hexdigest(),file=str(destination.relative_to(a.project)),sha256=hashlib.sha256(destination.read_bytes()).hexdigest()))
                print(f'Exported {cid} {name}: {count if enabled else 0} original lamp instances in {len(insertions)} unchanged base assemblies.',flush=True)
    (output/'manifest.json').write_text(json.dumps(dict(source_image_sha256=IMAGE_HASH,records=records,boundaries='Original complete primary/static selectors execute. Matrix/graphics boundaries are captured; all lamp culling queries admit geometry for native clipping. Other dynamic owners remain excluded. Lamp billboard matrix reset is separately compared against original instructions.'),indent=2)+'\n')
if __name__=='__main__':main()
