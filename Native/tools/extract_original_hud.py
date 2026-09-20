#!/usr/bin/env python3
"""Convert the user's original race HUD banks, retaining authored chunk IDs."""
from pathlib import Path
import argparse,hashlib,json
import extract_original_models as models
import texture_bank

BANKS=[('game2d','game2d'),('game2d','game2d_ta'),('game2d','game2d_vs'),
       ('game2d','game2d_intr'),('v3sG00game2d','v3sG00game2d'),('shift','shift'),('start2d','start2d')]
def sha(b):return hashlib.sha256(b).hexdigest()
def export(hostfs:Path,out:Path):
    entries=[]
    for folder,prefix in BANKS:
        source=hostfs/'model'/folder
        _,chunks,sources,polygon=models.parse_model(source,prefix+'_pol.tbl',prefix+'_tex.tbl')
        dst=out/prefix;dst.mkdir(parents=True,exist_ok=True)
        texture_source=source/(prefix+'_tex.bin.nz')
        if not texture_source.exists():texture_source=source/(prefix+'_tex.bin')
        texture_bytes=models.read_payload(texture_source)
        actual_input=texture_source
        if texture_bytes!=texture_source.read_bytes():
            actual_input=dst/'decoded_texture.bin';actual_input.write_bytes(texture_bytes)
        tex=texture_bank.export_bank(sources['texture_table'],actual_input,dst/'textures','twiddled')
        sources['texture_payload']=texture_source
        models.write_binary(dst/(prefix+'.idasmesh'),chunks)
        catalog=[]
        for c in chunks:
            vertices=[v for batch in c['batches'] for v in batch['vertices']]
            catalog.append({'index':c['index'],'source_offset':c['source_offset'],'source_size':c['source_size'],
              'bounds_xyz':[[min(v[i] for v in vertices),max(v[i] for v in vertices)] for i in [1,2,3]] if vertices else [],
              'texture_indices':sorted(set(batch['material'][9] for batch in c['batches'])),
              'vertices':len(vertices),'triangles':sum(len(batch['indices'])//3 for batch in c['batches']),
              'batches':[{'gmp_hex':[f'{w:08x}' for w in batch['material']],
                           'ich_hex':[f'{w:08x}' for w in batch['words']],
                           'vertex_source_sha256':sha(batch['source_vertex_bytes'])} for batch in c['batches']]})
        result={'schema':'idas3-original-hud-bank-v1','bank':prefix,'mesh':prefix+'.idasmesh',
          'texture_pack':'textures/textures.idastex','chunks':catalog,'texture_count':len(tex['textures']),
          'source_files':{k:{'path':str(p),'bytes':p.stat().st_size,'sha256':sha(p.read_bytes())} for k,p in sources.items()},
          'decoded_polygon_sha256':sha(polygon),'coordinate_transform':'none','uv_transform':'none',
          'scope':'Original authored geometry and textures only. Draw selection/layout is recovered separately; this file does not assign inferred meanings.'}
        (dst/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
        entries.append({'bank':prefix,'chunks':len(chunks),'textures':len(tex['textures']),'manifest':prefix+'/manifest.json'})
    result={'schema':'idas3-original-hud-index-v1','banks':entries}
    (out/'index.json').write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();print(json.dumps(export(a.hostfs,a.out),indent=2))
