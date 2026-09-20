#!/usr/bin/env python3
"""Export the source result preview's floor and selector3 environment texels."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_menus import export_model_menu,ORIGINAL_IMAGE_SHA256
import texture_bank
def export(image,hostfs,root):
    code=image.read_bytes()
    if hashlib.sha256(code).hexdigest()!=ORIGINAL_IMAGE_SHA256:raise ValueError('Original image identity')
    out=root/'data/original_assets/tuning'
    floor=export_model_menu(hostfs/'model/selcrs2',out/'selcrs2','twiddled')
    source=hostfs/'binary/j_env_select128_b.bin.nz';raw=source.read_bytes()
    if len(raw)!=128*128*2:raise ValueError('Original environment extent')
    #029DDC..029DE6 supplies128x128, format1,flags1 to1F7020.
    pixels=texture_bank.decode(raw,128,128,1,'twiddled')
    env=out/'environment';env.mkdir(parents=True,exist_ok=True)
    texture_bank.write_native_pack(env/'textures.idastex',[dict(index=0,width=128,height=128,rgba=pixels)])
    texture_bank.write_png(env/'environment.png',128,128,pixels)
    manifest={'schema':'idas3-original-tuning-preview-v1','original_sha256':ORIGINAL_IMAGE_SHA256,
      'constructor':'077C80 with r5=0 from06FB38 selects model/selcrs2',
      'draw':'078720 chunks0,3; ACar normal/reflected;070E4A supplied matrixT(0,-.4,-6)RX1024',
      'environment':{'source':str(source),'sha256':hashlib.sha256(raw).hexdigest(),
       'dimensions':[128,128],'format':1,'flags':1,'selector':3,
       'loader':'029DA0, source array2EF204[3], descriptor029DDC..DE6 to1F7020',
       'binding':'026CBC..D02 finds the last material texture of semantic140 for ACar+1712;029DA0 replaces that texture image',
       'rgba_sha256':hashlib.sha256(pixels).hexdigest(),'roundtrip':'Every original16-bit texel checked by inverse packing'},
      'private_original_assets':True}
    (out/'preview_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--image',type=Path,required=True);p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True)
    a=p.parse_args();print(json.dumps(export(a.image,a.hostfs,a.project),indent=2))
