#!/usr/bin/env python3
"""Convert original course banks selected by retail042700's path table."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_models import parse_model,write_binary
from texture_bank import export_bank
from course_palette_bank import COURSE_PALETTES,export_palette_bank
from export_original_assembly import IMAGE_HASH

COURSES=['k_ez','s_nm','h_hd','k_df','s_vh','s_uh','n_sy','k_tu','k_df3']
def digest(p):return {'path':str(p.resolve()),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
def main():
    p=argparse.ArgumentParser(description=__doc__)
    for n in ('image','hostfs','project'):p.add_argument('--'+n,type=Path,required=True)
    p.add_argument('--courses',default='0,1,2');p.add_argument('--weather',default='0');a=p.parse_args()
    raw=a.image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical image mismatch')
    for course in map(int,a.courses.split(',')):
        if not 0<=course<len(COURSES):raise ValueError('Original course index outside catalog')
        cid=COURSES[course];out=a.project/'data/original_models/courses'/cid;out.mkdir(parents=True,exist_ok=True)
        texout=a.project/'data/original_assets/courses'/cid
        done_models={};done_textures={};variants=[]
        def source(path):
            if not path.startswith('/driveA/'):raise ValueError('Unexpected original source path')
            return a.hostfs/path[len('/driveA/'):]
        def texture(path):
            if path in done_textures:return done_textures[path]
            stem=source(path);directory=texout/stem.name
            table=stem.with_suffix('.tbl').read_bytes()
            formats=sorted({struct.unpack_from('<HHBBHII',table,i)[2] for i in range(0,len(table),16)})
            if formats==[7] and course in COURSE_PALETTES and stem.name.endswith('_etc_r_tex'):
                meta=export_palette_bank(stem.with_suffix('.tbl'),stem.with_suffix('.bin.nz'),directory,a.image,COURSE_PALETTES[course])
                result={'file':str((directory/'textures.idastex').relative_to(a.project)).replace('\\','/'),'count':1,'palette_id':meta['palette_id'],'palette_address':meta['palette_address']}
                done_textures[path]=result;return result
            if any(fmt not in (0,1,2) for fmt in formats):
                result={'file':None,'formats':formats,'unavailable_reason':'Original texture format not decoded yet; no replacement pixels used.'}
                done_textures[path]=result;return result
            meta=export_bank(stem.with_suffix('.tbl'),stem.with_suffix('.bin.nz'),directory,'twiddled')
            result={'file':str((directory/'textures.idastex').relative_to(a.project)).replace('\\','/'),'count':len(meta['textures'])}
            done_textures[path]=result;return result
        def model(path,texture_path):
            if path in done_models:return done_models[path]
            stem=source(path);tex=source(texture_path).with_suffix('.tbl')
            tables=[stem.with_suffix('.tbl')] if stem.with_suffix('.tbl').exists() else [stem.parent/(stem.name+'_'+bank+'.tbl') for bank in 'abc']
            chunks=[];sources=[];counts=[]
            for table in tables:
                _,items,files,payload=parse_model(table.parent,table.name,str(tex.resolve()))
                counts.append(len(items));first=len(chunks)
                for item in items:item['index']=len(chunks);chunks.append(item)
                sources.append({'table':digest(table),'payload':digest(files['polygon']),'decoded_sha256':hashlib.sha256(payload).hexdigest(),'first_chunk':first,'count':len(items)})
            target=out/'banks'/(stem.name+'.idasmesh');target.parent.mkdir(exist_ok=True);write_binary(target,chunks)
            bounds=[]
            for chunk in chunks:
                vs=[v for batch in chunk['batches'] for v in batch['vertices']]
                bounds.append({'index':chunk['index'],'batches':len(chunk['batches']),'bounds':[[min(v[k] for v in vs),max(v[k] for v in vs)] for k in (1,2,3)] if vs else None})
            result={'file':str(target.relative_to(a.project)).replace('\\','/'),'count':len(chunks),'bank_counts':counts,'sha256':hashlib.sha256(target.read_bytes()).hexdigest(),'sources':sources,'chunks':bounds}
            done_models[path]=result;return result
        for night in (0,1):
            for weather in map(int,a.weather.split(',')):
                if weather not in (0,1):raise ValueError('Original weather selector outside two records')
                address=0xc2eff40+(course*2+night)*1024+weather*512
                paths=[raw[address-0xc020000+i*64:address-0xc020000+(i+1)*64].split(b'\0',1)[0].decode('ascii') for i in range(8)]
                print(f'{cid} night{night} weather{weather}: {paths[0]}',flush=True)
                world=model(paths[0],paths[1]);background=model(paths[2],paths[3])
                variants.append({'night':night,'weather':weather,'source_record':f'{address:08X}','paths':paths,'model':world['file'],'world_bank_counts':world['bank_counts'],'world_chunk_count':world['count'],'textures':texture(paths[1]),'background':background['file'],'background_chunk_count':background['count'],'background_textures':texture(paths[3])})
        manifest={'schema':'idas3-course-bank-roster-v1','course':cid,'scene_course_index':course,'source_image_sha256':IMAGE_HASH,'coordinate_transform':'none; source XYZ/UV and material words retained','models':done_models,'textures':done_textures,'variants':variants,'selection_status':'Geometry imported; scene load metadata is published only after original render selection is captured.'}
        (out/'banks_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
        print(f'Converted {cid}: {len(done_models)} original model banks, {len(done_textures)} texture banks',flush=True)

if __name__=='__main__':main()

