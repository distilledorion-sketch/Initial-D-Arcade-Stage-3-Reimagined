import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_models import parse_model,write_binary
from texture_bank import export_bank
from export_original_demo import CANONICAL
from export_course_selections import text
from export_original_assembly import identity

def main():
 p=argparse.ArgumentParser()
 for n in ('image','hostfs','project'):p.add_argument(n,type=Path)
 a=p.parse_args();raw=a.image.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=CANONICAL:raise ValueError('Canonical image mismatch')
 address=0xc23eae3;paths=[raw[address-0xc020000+i*64:address-0xc020000+(i+1)*64].split(b'\0',1)[0].decode('ascii') for i in range(8)]
 if paths[0]!='/driveA/model/course/k_dfa/dfa_pol':raise ValueError('Demo scene path changed')
 target=a.project/'data/original_models/attract/demo';target.mkdir(parents=True,exist_ok=True)
 report={'source_image_sha256':CANONICAL,'scene':20,'reverse':0,'night':1,'weather':0,'path_record':hex(address),'paths':paths,'models':[]}
 for i in (0,2):
  stem=a.hostfs/paths[i][8:];texture=a.hostfs/paths[i+1][8:];tables=[stem.with_suffix('.tbl')] if stem.with_suffix('.tbl').exists() else [stem.parent/(stem.name+'_'+b+'.tbl') for b in 'abc'];chunks=[];counts=[]
  for table in tables:
   _,items,_,_=parse_model(table.parent,table.name,str(texture.with_suffix('.tbl').resolve()));counts.append(len(items))
   for item in items:item['index']=len(chunks);chunks.append(item)
  dest=target/(stem.name+'.idasmesh');write_binary(dest,chunks)
  texdir=a.project/'data/original_assets/attract/demo'/texture.name;meta=export_bank(texture.with_suffix('.tbl'),texture.with_suffix('.bin.nz'),texdir,'twiddled')
  report['models'].append({'file':str(dest.relative_to(a.project)).replace('\\','/'),'chunks':len(chunks),'bank_counts':counts,'textures':str((texdir/'textures.idastex').relative_to(a.project)).replace('\\','/'),'texture_count':len(meta['textures']),'sha256':hashlib.sha256(dest.read_bytes()).hexdigest()})
  print(stem.name,len(chunks),'chunks',len(meta['textures']),'textures',flush=True)
 (target/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
 capture=target/'course_capture.json'
 if capture.exists():
  source=json.loads(capture.read_text());folder=target/'assemblies';folder.mkdir(exist_ok=True);files=[]
  if source['scene_index']!=20 or source['path_count']!=4089:raise ValueError('Wrong demo selection capture')
  for i,draws in enumerate(source['assemblies']):
   file=folder/f'{i:03d}.idasasm'
   with file.open('wb') as out:
    out.write(b'IDAS3A1\0'+struct.pack('<II',1,len(draws)))
    for draw in draws:out.write(struct.pack('<I16f',draw['chunk'],*draw['matrix']))
   files.append(str(file.relative_to(a.project)).replace('\\','/'))
  with (target/'scene.idasscene').open('wb') as out:
   out.write(b'ID3SCN1\0'+struct.pack('<I',1))
   for value in (report['models'][0]['file'],report['models'][0]['textures'],report['models'][1]['file'],report['models'][1]['textures']):text(out,value)
   out.write(struct.pack('<II',len(files),len(source['changes'])))
   for file in files:text(out,file)
   for start,index in source['changes']:out.write(struct.pack('<II',start,index))
   out.write(struct.pack('<I',1)+struct.pack('<I16f',source['background_chunk'],*identity()))
  source['source_image_sha256']=CANONICAL
  source['capture_scope']='Original03FCE0 primary and03FEA0 static selectors at every4089-point path index. Bounded submission/matrix/path dependencies; no hardware or whole-game emulation.'
  source['limitations']=['Original fog, dynamic light setup and separately owned objects are not part of this geometry selection capture.','Current view identity and source background XZ anchoring are reconstructed by native renderer.']
  capture.write_text(json.dumps(source,indent=2)+'\n')
  print('Published original dedicated demo scene',flush=True)
if __name__=='__main__':main()
