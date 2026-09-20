"""Preserve original road-projected headlight model, textures and mapping."""
from pathlib import Path
import argparse,hashlib,json,struct
from extract_original_menus import export_model_menu
from extract_original_models import parse_model
IMAGE_SHA='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path)
    a=p.parse_args()
    if hashlib.sha256(a.image.read_bytes()).hexdigest()!=IMAGE_SHA:raise ValueError('Unexpected canonical image')
    out=a.project/'data/original_assets/headlight_projection';out.mkdir(parents=True,exist_ok=True)
    bank=export_model_menu(a.hostfs/'model/lightobj',out,'twiddled')
    # The reusable menu exporter has a title-quad diagnostic description. It
    # does not describe this independent world-space projected mesh.
    bank['projection_evidence']='Chunk7 is selected by original034FA0, published by0D84A0 and independently submitted by034E60. Its nine projected world-space points come from0D50A0; no title-quad fitting is used.'
    bank['known_unresolved']=['Other lightobj chunks are preserved but not interpreted by this component.',
                              'Outer camera traversal timing and complete graphics submission are separate integration dependencies.']
    (out/'manifest.json').write_text(json.dumps(bank,indent=2)+'\n')
    _,chunks,_,payload=parse_model(a.hostfs/'model/lightobj');c=chunks[7]
    if len(c['batches'])!=1 or len(c['batches'][0]['vertices'])!=12 or c['batches'][0]['words'][6]!=74:raise ValueError('Unexpected projection model')
    (out/'source_chunk7.bin').write_bytes(payload[c['source_offset']:c['source_offset']+c['source_size']])
    records={}
    for ext,expected in [('bin',288),('tbl',48)]:
        src=a.hostfs/'binary'/('k_light_test8.'+ext);raw=src.read_bytes()
        if len(raw)!=expected:raise ValueError('Unexpected original projection mapping')
        (out/src.name).write_bytes(raw);records[src.name]={'source':str(src),'bytes':len(raw),'sha256':hashlib.sha256(raw).hexdigest()}
    report={'source_image_sha256':IMAGE_SHA,'source_owner':'ACar+2636;034FA0 -> 0D4C80;034E60 draw then projection update',
            'chunk':7,'projection_points':9,'strip_vertices':12,'model_bank':bank,'mapping':records,
            'scope':'Original geometry,texels,UV,material and mapping records retained. Road-query inputs and results are separate source dependencies; no replacement beam artwork.'}
    (out/'projection_manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Exported original lightobj chunk7,9-point projection mapping and original textures.')
if __name__=='__main__':main()
