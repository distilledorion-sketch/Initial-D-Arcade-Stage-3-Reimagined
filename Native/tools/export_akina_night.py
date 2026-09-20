"""Export the original scene7 Akina variant, preserving model/texture data."""
import argparse,hashlib,json
from pathlib import Path
from extract_original_models import parse_model,write_binary
from texture_bank import export_bank

def main():
    p=argparse.ArgumentParser();p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True);a=p.parse_args()
    source=a.hostfs/'model/course/k_df2';out=a.project/'data/original_models/courses/k_df/night';out.mkdir(parents=True,exist_ok=True)
    chunks=[];sources={}
    for bank,expected in zip('abc',[20,25,92]):
        _,items,files,_=parse_model(source,f'df2_pol_{bank}.tbl','../k_df/df_tex.tbl')
        if len(items)!=expected:raise ValueError('Scene7 chunk layout changed')
        for item in items:item['index']=len(chunks);chunks.append(item)
        for name,path in files.items():sources[f'{bank}_{name}']={'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    write_binary(out/'k_df_night.idasmesh',chunks)
    _,background,files,_=parse_model(source,'df2_etc_f_pol.tbl','df2_etc_f_tex.tbl')
    write_binary(out/'background.idasmesh',background)
    textures=a.project/'data/original_assets/courses/k_df/night/background'
    bank=export_bank(source/'df2_etc_f_tex.tbl',source/'df2_etc_f_tex.bin.nz',textures,'twiddled')
    for name,path in files.items():sources[f'background_{name}']={'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()}
    meta={'source_record':'0C2EFF40 + 7*1024, variant0','models':'Original df2 banks; 137 original indexed chunks','world_textures':'Original shared k_df/df_tex bank, unchanged','assembly':'Same indexed30sector assembly selection; extra chunks135/136 preserved pending original effects dispatch','background_texture_count':len(bank['textures']),'sources':sources,'changes':'None to XYZ,UV,materials,colors or texture pixels','limits':'Host night headlights/lighting remain presentation reconstruction'}
    (out/'manifest.json').write_text(json.dumps(meta,indent=2)+'\n');print(json.dumps({'chunks':len(chunks),'background_chunks':len(background),'background_textures':len(bank['textures'])}))
if __name__=='__main__':main()
