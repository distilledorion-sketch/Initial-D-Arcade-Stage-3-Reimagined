#!/usr/bin/env python3
"""Export source Sega/Rosso/Caution assets, including every Rosso polygon frame."""
import argparse
import hashlib
import json
from pathlib import Path
from extract_original_menus import export_model_menu
import extract_original_models as models

IMAGE_HASH='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def export(image,hostfs,project):
    if hashlib.sha256(image.read_bytes()).hexdigest()!=IMAGE_HASH:raise ValueError('Unexpected source image')
    root=project/'data/original_assets/attract/cards';root.mkdir(parents=True,exist_ok=True)
    records=[]
    for name in ['segalogo','segarosso2d','adv_caution']:
        bank=export_model_menu(hostfs/'model'/name,root/name,'twiddled')
        records.append({'name':name,'chunk_count':bank['chunk_count'],'texture_count':bank['texture_count'],'source_files':bank['source_files'],'mesh_pack_sha256':bank['mesh_pack_sha256']})
    # The source Rosso polygon animation has an intentionally EMPTY texture
    # table. It uses authored flat material colors; no dummy texture is added.
    prefix,chunks,sources,polygon=models.parse_model(hostfs/'model/segarosso')
    if sources['texture_table'].read_bytes():raise ValueError('Unexpected Rosso texture table')
    if len(chunks)!=211:raise ValueError('Unexpected Rosso animation count')
    for chunk in chunks:
        for batch in chunk['batches']:
            if batch['material'][9]!=0xffffffff:raise ValueError('Unexpected textured Rosso batch')
    folder=root/prefix;folder.mkdir(exist_ok=True)
    models.write_binary(folder/(prefix+'.idasmesh'),chunks)
    (folder/'original_pol.bin').write_bytes(polygon)
    (folder/'original_pol.tbl').write_bytes(sources['table'].read_bytes())
    records.append({'name':prefix,'chunk_count':len(chunks),'texture_count':0,'source_files':{k:{'path':str(v),'sha256':hashlib.sha256(v.read_bytes()).hexdigest()} for k,v in sources.items()},'mesh_pack_sha256':hashlib.sha256((folder/(prefix+'.idasmesh')).read_bytes()).hexdigest()})
    report={'schema':'idas3-original-attract-cards-v1','source_image_sha256':IMAGE_HASH,'banks':records,'source_owners':{'3':'0743A0/0746E0','4':'073660/073960','5':'0398E0/039BC0'},'transformation':'Original polygon positions/materials/UVs preserved; textured banks decode/repack every source texel; no replacement artwork.'}
    (root/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Exported214 original card chunks and3 texture banks, including all211 Rosso polygons')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path)
    a=p.parse_args();export(a.image,a.hostfs,a.project)
