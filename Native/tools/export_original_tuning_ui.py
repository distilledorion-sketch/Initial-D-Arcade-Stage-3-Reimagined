"""Export original tuning banks and the exact option-font/JIS spacing data."""
import argparse,hashlib,pathlib,struct
import texture_bank
from extract_original_menus import export_model_menu
from arcade_program import identify, JAPANESE, BASE

def export(image_path,hostfs,out):
    image=pathlib.Path(image_path).read_bytes()
    layout=identify(image)
    hostfs=pathlib.Path(hostfs);out=pathlib.Path(out)
    for name in ['v3sK12shop','v3sK17continue']:export_model_menu(hostfs/'model'/name,out/name,'twiddled')
    source=hostfs/'font/option';target=out/'option'
    texture_bank.export_bank(source/'option_spr.tbl',source/'option_spr.bin.nz',target/'textures','twiddled')
    (target/'font.bin').write_bytes((source/'option_font.bin.nz').read_bytes())
    spacing=bytearray()
    spacing_table=0xc344c8c if layout==JAPANESE else 0xc33919c
    for index in range(35):
        ptr,trim=struct.unpack_from('<II',image,spacing_table+index*8-BASE)
        code=struct.unpack_from('<H',image,ptr-0xc020000)[0]
        spacing.extend(struct.pack('<HHI',code,0,trim))
    (target/'spacing.bin').write_bytes(spacing)
    print('Exported original tuning banks, option glyphs, 9216 JIS indices and 35 spacing records')
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image');p.add_argument('hostfs');p.add_argument('out');a=p.parse_args();export(a.image,a.hostfs,a.out)
