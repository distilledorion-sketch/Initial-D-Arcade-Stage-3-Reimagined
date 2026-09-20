#!/usr/bin/env python3
"""Lossless iStart2D namekana font, name strings and motion-table export."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import texture_bank

CANONICAL = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
BASE = 0x0C020000
# These are transcriptions of the English text painted by the original
# namekana images, not translations of the Japanese lookup-key bytes.
LOCALIZED = ['IGGY','KENJI','SHINGO','TORU','KAWAI','MAYA & SIMONE',
    'TWO GUYS FROM TOKYO','DANNY','K.T.','COLE','ZACK','KYLE','RY','TAK',
    'HAWK','KYLIE','CAINE','MIKI','DICE','SMILEY','TOUCH','NOBU','SID','AKI',
    'KYLIE','RY','MAN IN EVO.V','MAN IN EVO.VI','K.T.','TAK','BUNTA','BUNTA']

def export(image: Path, source: Path, profile: Path, output: Path):
    program = image.read_bytes()
    if hashlib.sha256(program).hexdigest() != CANONICAL:
        raise ValueError('Unknown original source image')
    def u32(address): return struct.unpack_from('<I', program, address-BASE)[0]
    def cstring(address):
        begin = address-BASE
        return program[begin:program.index(b'\0', begin)]
    names, records = [], []
    for enemy in range(32):
        lines = []
        for line in range(2):
            address = u32(0x0C33B780 + (enemy*2+line)*4)
            name = cstring(address)
            if len(name) >= 16 or len(name)%2: raise ValueError('Invalid original start name')
            names.append(name.ljust(16, b'\0'))
            lines.append({'address': f'{address:08X}', 'bytes': name.hex(), 'unicode': name.decode('euc_jp')})
        records.append({'enemy': enemy, 'lines': lines, 'rendered_english':LOCALIZED[enemy]})
    indices = (source/'namekana_font.bin.nz').read_bytes()
    if len(indices) != 9216*2: raise ValueError('Invalid namekana index count')
    unicode = []
    for index in range(9216):
        try:
            text = bytes([index//96+160, index%96+160]).decode('euc_jp')
            unicode.append(ord(text) if len(text)==1 else 0)
        except UnicodeDecodeError: unicode.append(0)
    profile_bytes = profile.read_bytes()
    if struct.unpack_from('<3I', profile_bytes) != (0x454e4449, 1, 221):
        raise ValueError('Invalid source profile glyph table')
    # 1917A0 returns6 and1917C0 returns this six-glyph-ID array. The HUD's
    # default Japanese-name key at33B88C belongs to another font and is blank
    # in namekana. This is the source start sequence's actual PLAYER fallback.
    default_ids=struct.unpack_from('<6I',program,0x0C298958-BASE)
    default=b''.join(profile_bytes[20+2*g:22+2*g] for g in default_ids).ljust(16,b'\0')
    animation = program[0x0C31EE90-BASE:0x0C31EE90-BASE+80*12]
    output.mkdir(parents=True, exist_ok=True)
    bank = texture_bank.export_bank(source/'namekana_spr.tbl', source/'namekana_spr.bin.nz', output/'textures', 'twiddled')
    payload = (b'IDAS3SN1' + struct.pack('<5I',1,len(bank['textures']),9216,221,80) +
        indices + struct.pack('<9216I', *unicode) + profile_bytes[20:20+442] +
        b''.join(names) + default + animation)
    (output/'start_names.bin').write_bytes(payload)
    sources = [image, source/'namekana_font.bin.nz', source/'namekana_spr.tbl', source/'namekana_spr.bin.nz', profile]
    manifest = {'schema':'idas3-original-start-names-v1', 'private_original_assets':True,
        'sources':[{'path':str(p.resolve()),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sources],
        'output':str((output/'start_names.bin').resolve()), 'output_sha256':hashlib.sha256(payload).hexdigest(),
        'font_count':len(bank['textures']), 'source_rules': '14BEC0 namekana 64px; 191940 table33B780[enemy][line]; 14C1E0 names at (224-32*n,104) and (416-32*n,312), two opponent lines y280/344; 14B700 animation31EE90 with 80 x/y/alpha triplets; VS41..70 after 100 phase ticks.',
        'localization':'Japanese EUC-JP values are texture lookup keys; localized namekana rival textures themselves paint English. rendered_english is a visual transcription. General profile glyphs retain their normal text.',
        'default_player_glyph_ids':default_ids, 'default_player_bytes':default.hex(),
        'motion_address':'0C31EE90', 'motion_sha256':hashlib.sha256(animation).hexdigest(),
        'names':records, 'motion_samples':[list(struct.unpack_from('<3f',animation,i*12)) for i in range(80)]}
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2,ensure_ascii=True)+'\n',encoding='utf-8')
    return manifest

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('image','source','profile','out'): parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    result=export(args.image,args.source,args.profile,args.out)
    print(f"Exported {result['font_count']} original 64px glyph textures and exact source name motion")
