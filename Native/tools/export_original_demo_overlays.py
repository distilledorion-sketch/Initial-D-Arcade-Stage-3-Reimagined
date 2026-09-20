#!/usr/bin/env python3
"""Export DemoMain's original 206 2D cues, six fades and adv2d_v3 artwork."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_menus import export_model_menu
IMAGE_HASH='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def export(image,hostfs,project):
    raw=image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=IMAGE_HASH:raise ValueError('Unexpected source image')
    def words(address,count):return struct.unpack_from('<'+'I'*count,raw,address-0xc020000)
    def cstring(address):return raw[address-0xc020000:].split(b'\0')[0].decode('ascii')
    for variant in range(3):
        assert words(0xc3129d0+variant*8,2)==(0xc30f27c,206)
        assert words(0xc3129e8+variant*8,2)==(0xc30f27c,0)
        assert words(0xc312a00+variant*8,2)==(0xc312934,6)
        assert cstring(words(0xc312a24+variant*20,1)[0])=='/driveA/model/adv2d/adv2d_v3_pol'
    root=project/'data/original_assets/attract/demo_overlays';root.mkdir(parents=True,exist_ok=True)
    bank=export_model_menu(hostfs/'model/adv2d',root/'adv2d_v3','twiddled')
    assert bank['name']=='adv2d_v3'
    cues=raw[0xc30f27c-0xc020000:0xc30f27c-0xc020000+206*68]
    fades=raw[0xc312934-0xc020000:0xc312934-0xc020000+6*24]
    (root/'timeline.bin').write_bytes(b'IDO2D001'+struct.pack('<II',206,6)+cues+fades)
    rows=[]
    for i in range(206):
        r=struct.unpack_from('<5I12f',cues,i*68)
        assert r[0]<bank['chunk_count'] and r[1]<r[4]
        rows.append(dict(row=i,chunk=r[0],frames=r[1:5],positionFrom=r[5:8],positionTo=r[8:11],scaleFrom=r[11:14],scaleTo=r[14:17]))
    report=dict(schema='idas3-original-demo-overlays-v1',source_image_sha256=IMAGE_HASH,
        source_main='0C0E7648..0C0E7CCE',cue_table='0C30F27C',fade_table='0C312934',
        variants='All three source variants select identical tables and adv2d_v3 assets',
        second_overlay_count=0,bank={k:bank[k] for k in ['chunk_count','texture_count','mesh_pack_sha256','source_files']},
        cues=rows,fades=[struct.unpack_from('<5If',fades,i*24) for i in range(6)])
    (root/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"Exported {len(rows)} original overlay cues, six fades, {bank['chunk_count']} polygons and {bank['texture_count']} textures")
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path)
    a=p.parse_args();export(a.image,a.hostfs,a.project)
