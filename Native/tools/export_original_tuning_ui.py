"""Export original tuning banks and the exact option-font/JIS spacing data."""
import argparse,hashlib,pathlib,struct
import texture_bank
from extract_original_menus import export_model_menu
from export_original_tuning import CANONICAL_SHA

def export(image_path,hostfs,out):
    image=pathlib.Path(image_path).read_bytes()
    if hashlib.sha256(image).hexdigest()!=CANONICAL_SHA:raise ValueError('Canonical program identity mismatch')
    hostfs=pathlib.Path(hostfs);out=pathlib.Path(out)
    for name in ['v3sK12shop','v3sK17continue']:export_model_menu(hostfs/'model'/name,out/name,'twiddled')
    source=hostfs/'font/option';target=out/'option'
    texture_bank.export_bank(source/'option_spr.tbl',source/'option_spr.bin.nz',target/'textures','twiddled')
    (target/'font.bin').write_bytes((source/'option_font.bin.nz').read_bytes())
    spacing=bytearray()
    for index in range(35):
        ptr,trim=struct.unpack_from('<II',image,0xc33919c+index*8-0xc020000)
        code=struct.unpack_from('<H',image,ptr-0xc020000)[0]
        spacing.extend(struct.pack('<HHI',code,0,trim))
    (target/'spacing.bin').write_bytes(spacing)
    print('Exported original tuning banks,40 option-font glyphs,9216 JIS indices and35 source spacing records')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('hostfs');p.add_argument('out');a=p.parse_args();export(a.image,a.hostfs,a.out)
