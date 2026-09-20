"""Preserve name-entry glyph, keyboard and substitution tables from game bytes."""
from pathlib import Path
import hashlib,struct,json
ROOT=Path(__file__).resolve().parents[1]
IMAGE=Path(r'C:/Users/Developer/Documents/Codex/Initial D Arcade Stage 3 Recompiled - Crash Safe Test/game files/idas3_main_0C020000.bin')
SHA='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    b=IMAGE.read_bytes();assert hashlib.sha256(b).hexdigest()==SHA
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
        value=string(u(0xc268d54+4*i));assert len(value)==2;glyphs.append(value)
    substitutions=[]
    for i in range(512):
        original=string(u(0xc31bcc8+8*i))
        if not original:break
        replacement=string(u(0xc31bccc+8*i));assert len(original)%2==len(replacement)%2==0
        substitutions.append((original,replacement))
    else:raise ValueError('Unbounded source name replacement table')
    keyboard=read(0xc2690c8,173*24)
    payload=struct.pack('<4sIIII',b'IDNE',1,221,173,len(substitutions))+b''.join(glyphs)+keyboard
    for a,b in substitutions:payload+=struct.pack('<II',len(a),len(b))+a+b
    dest=ROOT/'data/original_frontend/name_entry.bin';dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(payload)
    out=ROOT/'verification/original-name-entry';out.mkdir(parents=True,exist_ok=True)
    (out/'source-tables.json').write_text(json.dumps(dict(imageSha256=SHA,assetSha256=hashlib.sha256(payload).hexdigest(),glyphs=221,keyboardRecords=173,substitutions=len(substitutions),sourceAddresses=dict(glyphPointers='0C268D54',keyboard='0C2690C8',substitutions='0C31BCC8')),indent=2))
    print('PASS221 source glyphs,173 keyboard records,',len(substitutions),'substitutions')
if __name__=='__main__':main()
