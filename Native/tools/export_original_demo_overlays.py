#!/usr/bin/env python3
"""Export DemoMain's original 206 2D cues, six fades and adv2d_v3 artwork."""
import argparse,hashlib,json,struct
from pathlib import Path
from extract_original_menus import export_model_menu
from arcade_program import identify,JAPANESE,BASE
IMAGE_HASH='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def export(image,hostfs,project):
    raw=image.read_bytes()
    layout=identify(raw)
    delta=0x6ac if layout==JAPANESE else 0
    cues_at=0xc30f27c+delta
    fades_at=0xc312934+delta
    def words(address,count):return struct.unpack_from('<'+'I'*count,raw,address-0xc020000)
    def cstring(address):return raw[address-0xc020000:].split(b'\0')[0].decode('ascii')
    for variant in range(3):
        assert words(0xc3129d0+delta+variant*8,2)==(cues_at,206)
        assert words(0xc3129e8+delta+variant*8,2)==(cues_at,0)
        assert words(0xc312a00+delta+variant*8,2)==(fades_at,6)
        assert cstring(words(0xc312a24+delta+variant*20,1)[0])=='/driveA/model/adv2d/adv2d_v3_pol'
    root=project/'data/original_assets/attract/demo_overlays';root.mkdir(parents=True,exist_ok=True)
    bank=export_model_menu(hostfs/'model/adv2d',root/'adv2d_v3','twiddled','adv2d_v3_pol.tbl','adv2d_v3_tex.tbl')
    assert bank['name']=='adv2d_v3'
    cues=raw[cues_at-BASE:cues_at-BASE+206*68]
    fades=raw[fades_at-BASE:fades_at-BASE+6*24]
    (root/'timeline.bin').write_bytes(b'IDO2D001'+struct.pack('<II',206,6)+cues+fades)
    rows=[]
    for i in range(206):
        r=struct.unpack_from('<5I12f',cues,i*68)
        assert r[0]<bank['chunk_count'] and r[1]<r[4]
        rows.append(dict(row=i,chunk=r[0],frames=r[1:5],positionFrom=r[5:8],positionTo=r[8:11],scaleFrom=r[11:14],scaleTo=r[14:17]))
    report=dict(schema='idas3-original-demo-overlays-v1',source_image_sha256=layout.sha256,
        source_main='0C0E7648..0C0E7CCE',cue_table=f'{cues_at:08X}',fade_table=f'{fades_at:08X}',
        variants='All three source variants select identical tables and adv2d_v3 assets',
        second_overlay_count=0,bank={k:bank[k] for k in ['chunk_count','texture_count','mesh_pack_sha256','source_files']},
        cues=rows,fades=[struct.unpack_from('<5If',fades,i*24) for i in range(6)])
    (root/'manifest.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"Exported {len(rows)} original overlay cues, six fades, {bank['chunk_count']} polygons and {bank['texture_count']} textures")
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('project',type=Path)
    a=p.parse_args();export(a.image,a.hostfs,a.project)
