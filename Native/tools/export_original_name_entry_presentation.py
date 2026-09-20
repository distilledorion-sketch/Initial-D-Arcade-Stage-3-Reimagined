"""Offline import of the original name-entry model banks and cursor records."""
from pathlib import Path
import argparse, hashlib, json, struct
from extract_original_menus import export_model_menu, ORIGINAL_IMAGE_SHA256
import extract_original_models as models
import texture_bank

def export(image: Path, hostfs: Path, root: Path):
    data=image.read_bytes()
    if hashlib.sha256(data).hexdigest()!=ORIGINAL_IMAGE_SHA256:
        raise ValueError('Canonical Stage3 image required')
    base=root/'data/original_assets/name_entry'
    manifests=[]
    for bank,name in [(16,'v3sS08name'),(32,'select0302')]:
        for table,suffix in [(0x0c2a9648,'pol'),(0x0c2a9c78,'tex')]:
            pointer=struct.unpack_from('<I',data,table-0x0c020000+bank*4)[0]
            actual=data[pointer-0x0c020000:].split(b'\0')[0].decode()
            expected=f'/driveA/model/{name}/{name}_{suffix}'
            if actual!=expected:raise ValueError('Original resource selector changed')
        result=export_model_menu(hostfs/'model'/name,base/name,'twiddled')
        manifests.append({'bank':bank,'name':name,'chunks':result['chunk_count'],'textures':result['texture_count']})
    # Legacy select includes intentionally empty chunks. Preserve them and
    # their original indices; the general menu catalog assumes nonempty quads.
    folder=hostfs/'model/select';legacy=base/'select';legacy.mkdir(parents=True,exist_ok=True)
    for address,suffix in [(0x0c266b08,'pol'),(0x0c266b28,'tex')]:
        actual=data[address-0x0c020000:].split(b'\0')[0].decode()
        if actual!=f'/driveA/model/select/select_{suffix}':raise ValueError('Legacy name resource changed')
    prefix,chunks,sources,polygon=models.parse_model(folder)
    models.write_binary(legacy/'select.idasmesh',chunks)
    raw_texture=models.read_payload(folder/'select_tex.bin.nz');(legacy/'original_texture_decoded.bin').write_bytes(raw_texture)
    tex=texture_bank.export_bank(folder/'select_tex.tbl',legacy/'original_texture_decoded.bin',legacy/'textures','twiddled')
    (legacy/'original_pol.bin').write_bytes(polygon);(legacy/'original_pol.tbl').write_bytes(sources['table'].read_bytes())
    legacy_manifest={'name':'select','source_polygon_sha256':hashlib.sha256(polygon).hexdigest(),'source_texture_sha256':hashlib.sha256(raw_texture).hexdigest(),'source_chunk_count':len(chunks),'drawn_chunks':[22,23],'source_chunk_vertex_counts':[sum(len(b['vertices'])for b in c['batches'])for c in chunks],'texture_count':len(tex['textures'])}
    (legacy/'manifest.json').write_text(json.dumps(legacy_manifest,indent=2)+'\n')
    manifests.append({'bank':'legacy wrapper1','name':'select','chunks':len(chunks),'textures':len(tex['textures'])})
    raw=data[0x0c33fce0-0x0c020000:0x0c33fce0-0x0c020000+63*12]
    (base/'cursor.bin').write_bytes(raw)
    manifest={'schema':'original-name-entry-presentation-v1','source_image_sha256':ORIGINAL_IMAGE_SHA256,
              'owner':'0C1263E0','draw':'0C128500','v3_children':['0C1B1300','0C1B1CA0'],
              'banks':manifests,'cursor_address':'0C33FCE0','cursor_bytes':len(raw),
              'cursor_sha256':hashlib.sha256(raw).hexdigest(),
              'projection':'Source V3 authored menu plane; shared native 640x480 compositor',
              'private_original_assets':True}
    (base/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('image',type=Path);parser.add_argument('hostfs',type=Path);parser.add_argument('root',type=Path)
    a=parser.parse_args();print(json.dumps(export(a.image,a.hostfs,a.root),indent=2))
