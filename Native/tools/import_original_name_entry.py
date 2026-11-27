"""Preserve name-entry glyph, keyboard and substitution tables from game bytes."""
from pathlib import Path
import hashlib,struct,json
ROOT=Path(__file__).resolve().parents[1]
IMAGE=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/idas3_main_0C020000.bin')
from arcade_program import identify
def export(image, root):
    b=image.read_bytes()
    layout=identify(b)
    names=layout.name_entry
    def read(at,n):
        off=at-0xc020000;assert 0<=off<=len(b)-n;return b[off:off+n]
    def u(at):return struct.unpack('<I',read(at,4))[0]
    def string(at):
        result=bytearray()
        for i in range(32):
            v=read(at+i,1)[0]
            if not v:return bytes(result)
            result.append(v)
        raise ValueError('Unbounded source name string')
    glyphs=[]
    for i in range(221):
        value=string(u(names.glyphs+4*i));assert len(value)==2;glyphs.append(value)
    substitutions=[]
    for i in range(512):
        original=string(u(names.substitutions+8*i))
        if not original:break
        replacement=string(u(names.substitutions+4+8*i));assert len(original)%2==len(replacement)%2==0
        substitutions.append((original,replacement))
    else:raise ValueError('Unbounded source name replacement table')
    keyboard=read(names.keyboard,173*24)
    payload=struct.pack('<4sIIII',b'IDNE',1,221,173,len(substitutions))+b''.join(glyphs)+keyboard
    for a,b in substitutions:payload+=struct.pack('<II',len(a),len(b))+a+b
    dest=root/'data/original_frontend/name_entry.bin';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(payload)
    out=root/'verification/original-name-entry';out.mkdir(parents=True,exist_ok=True)
    (out/'source-tables.json').write_text(json.dumps(dict(imageSha256=layout.sha256,assetSha256=hashlib.sha256(payload).hexdigest(),glyphs=221,keyboardRecords=173,substitutions=len(substitutions),sourceAddresses=dict(glyphPointers=f'{names.glyphs:08X}',keyboard=f'{names.keyboard:08X}',substitutions=f'{names.substitutions:08X}')),indent=2))
    print('PASS221 source glyphs,173 keyboard records,',len(substitutions),'substitutions')
if __name__=='__main__':
    import argparse
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--image',type=Path,default=IMAGE)
    parser.add_argument('--project',type=Path,default=ROOT)
    args=parser.parse_args()
    export(args.image,args.project)
