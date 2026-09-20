#!/usr/bin/env python3
"""Publish native course scenes only after original selector capture succeeds."""
import argparse,json,struct,subprocess
from pathlib import Path
from export_original_assembly import identity
from export_original_assembly import IMAGE_HASH
from extract_course_roster import COURSES
def text(f,s):
    data=s.encode('utf-8');f.write(struct.pack('<I',len(data))+data)
def main():
    p=argparse.ArgumentParser()
    for n in ('image','project','capture_exe'):p.add_argument('--'+n.replace('_','-'),type=Path,required=True)
    p.add_argument('--courses',default='0,1,2');a=p.parse_args()
    paths={r['id']:r for r in json.loads((a.project/'data/courses/manifest.json').read_text())['courses']}
    for ci in map(int,a.courses.split(',')):
        cid=COURSES[ci];base=a.project/'data/original_models/courses'/cid
        banks=json.loads((base/'banks_manifest.json').read_text())
        for variant in banks['variants']:
            night=variant['night']
            weather=variant.get('weather',0)
            if not variant['textures']['file'] or not variant['background_textures']['file']:
                print(f'Skipping unavailable {cid} night{night} weather{weather}: unsupported original texture format',flush=True)
                continue
            for reverse in (0,1):
                name=('night' if night else 'day')+('_reverse' if reverse else '_forward')+('_wet' if weather else '')
                capture=base/f'{name}_capture.json'
                path_id='k_df' if cid=='k_df3' else cid
                subprocess.run([str(a.capture_exe),str(a.image),str(ci*2+night),str(reverse),str(paths[path_id]['points']),str(variant['world_chunk_count']),str(capture),str(weather)],check=True)
                source=json.loads(capture.read_text());folder=base/'assemblies'/name;folder.mkdir(parents=True,exist_ok=True)
                source['source_image_sha256']=IMAGE_HASH
                source['weather']=weather
                scene_index=ci*2+night
                source['constructor_scene_index']=scene_index+int(scene_index!=8 and not night and weather==1)
                source['constructor_selection_evidence']='Original0427DC..27F0 switches daytime-rain scenes to the following constructor, except Happogahara scene8; time+52 and weather+56 remain independent.'
                source['capture_scope']='Original primary/static selectors at every authoring-path index; independent frame-zero object state for each sample.'
                source['explicit_hooks']=['world geometry interface submission','separate background lookup/submission','current rigid view identity and matrix helper boundaries','integer division helper','path point count','external dynamic objects/crowd helper submissions']
                source['limitations']=['Dynamic instance lists and separately owned crowd/tree objects are omitted.','Per-frame animated props retain the captured original frame-zero transform.','Derived matrix helpers preserve source operation order but do not claim hardware matrix bit parity.','Original GLM lighting, fog and rain effects are outside the asset selection capture.']
                capture.write_text(json.dumps(source,separators=(',',':'))+'\n')
                files=[]
                for i,draws in enumerate(source['assemblies']):
                    file=folder/f'{i:03d}.idasasm'
                    with file.open('wb') as out:
                        out.write(b'IDAS3A1\0'+struct.pack('<II',1,len(draws)))
                        for draw in draws:out.write(struct.pack('<I16f',draw['chunk'],*draw['matrix']))
                    files.append(str(file.relative_to(a.project)).replace('\\','/'))
                # Publish metadata last: available() never observes half-exported assets.
                destination=base/f'scene_{name}.idasscene';pending=destination.with_suffix('.pending')
                with pending.open('wb') as out:
                    out.write(b'ID3SCN1\0'+struct.pack('<I',1))
                    for value in [variant['model'],variant['textures']['file'],variant['background'],variant['background_textures']['file']]:text(out,value)
                    out.write(struct.pack('<II',len(files),len(source['changes'])))
                    for file in files:text(out,file)
                    for start,index in source['changes']:out.write(struct.pack('<II',start,index))
                    out.write(struct.pack('<I',1)+struct.pack('<I16f',source['background_chunk'],*identity()))
                pending.replace(destination)
                print(f'Published {cid} {name}',flush=True)
if __name__=='__main__':main()
