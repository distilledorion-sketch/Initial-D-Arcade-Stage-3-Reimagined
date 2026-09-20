"""Export Bunta Challenge's authored scripts and source-named artwork.

Static parsing only: these eight 51-row tables are already initialized in the
canonical image. No original instructions, graphics or devices are executed.
"""
from pathlib import Path
import argparse, hashlib, json, struct
import extract_original_models as models
import texture_bank

SHA256='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def export(image,hostfs,root):
    raw=image.read_bytes()
    if hashlib.sha256(raw).hexdigest()!=SHA256:raise ValueError('Canonical image required')
    def word(a):return struct.unpack_from('<I',raw,a-0x0c020000)[0]
    def string(a):
        if not 0x0c020000<=a<0x0c420000:raise ValueError(f'Invalid token address {a:x}')
        start=a-0x0c020000;end=raw.find(b'\0',start,start+4096)
        if end<0:raise ValueError('Unterminated original token')
        return raw[start:end]
    if string(0x0c263af4)!=b'/driveA/model/rival/r_bunta_challenge/rival_bunta_challenge_pol':raise ValueError('Portrait path differs')
    out=root/'data/original_assets/rival_dialog';out.mkdir(parents=True,exist_ok=True)
    data=bytearray(b'IDAS3BD1'+struct.pack('<3I',1,8,51));records=[]
    for course in range(8):
        for kind in range(51):
            address=0x0c363100+course*0x660+kind*32
            words=[word(address+i*4)for i in range(8)];script=words[0];cache={}
            def entry(j):
                if j>=512:raise ValueError('Script exceeds bounded token window')
                if j not in cache:
                    p=word(script+j*4);cache[j]=(p,string(p))
                return cache[j]
            # Original conditional scanners do not stop at E. Fifteen authored
            # rows have an omitted closing marker before their first E and
            # legitimately scan to the following marker in the pointer table.
            # Capture every reachable branch, not merely the first text page.
            closers={b'I':b'i',b'<':b'>',b'/':b'|',b'+':b'-'}
            pending=[0];seen=set()
            while pending:
                j=pending.pop()
                if j in seen:continue
                seen.add(j);t=entry(j)[1]
                if t==b'E':continue
                pending.append(j+1)
                if t in closers:
                    q=j
                    while entry(q)[1]!=closers[t]:q+=1
                    pending.append(q+1)
            tokens=[entry(j)for j in range(max(seen)+1)]
            data+=struct.pack('<10I',address,*words,len(tokens))
            for p,t in tokens:data+=struct.pack('<2I',p,len(t))+t
            records.append(dict(course=course,kind=kind,source_record=f'{address:08X}',source_script=f'{script:08X}',tokens=[dict(address=f'{p:08X}',bytes=t.decode('ascii'))for p,t in tokens]))
    (out/'bunta.idasdialog').write_bytes(data)
    # 0F4660/0F4D20 character31 uses these nineteen elements, and bypasses
    # the ordinary portrait part remap. Preserve source words for inspection.
    table=word(0x0c31a008+31*8);count=word(0x0c31a00c+31*8)
    if table!=0x0c319cc4 or count!=19:raise ValueError('Bunta element table differs')
    elements=[[word(table+j*44+i*4)for i in range(11)]for j in range(count)]
    name=string(word(word(0x0c31ba18+31*8)))
    generated='// Generated from canonical SHA256 '+SHA256+'; Bunta character31.\n'
    generated+='inline constexpr std::array<std::array<std::uint32_t,11>,19> buntaDialogElements{{\n'
    generated+=''.join('    {{'+','.join(f'0x{x:08x}u'for x in row)+'}},\n'for row in elements)+'}};\n'
    generated+='inline constexpr std::string_view buntaSpeakerName("'+''.join(f'\\x{x:02x}'for x in name)+'",'+str(len(name))+');\n'
    (root/'src/original_bunta_dialog_tables.inc').write_text(generated)
    jobs=[('rival/r_bunta_challenge','rival_bunta_challenge')]
    jobs += [(f'rival_bg/bg{c:02d}',f'rival_bg{c:02d}'+(''if c==8 else'nf'))for c in range(9)]
    banks=[]
    for folder,prefix in jobs:
        target=out/'banks'/prefix
        if (target/(prefix+'.idasmesh')).exists()and(target/'textures/textures.idastex').exists():
            if(target/'manifest.json').exists():banks.append(json.loads((target/'manifest.json').read_text()))
            continue
        source=hostfs/'model'/folder;target.mkdir(parents=True,exist_ok=True)
        _,chunks,sources,pol=models.parse_model(source,prefix+'_pol.tbl',prefix+'_tex.tbl')
        models.write_binary(target/(prefix+'.idasmesh'),chunks)
        (target/'original_pol.bin').write_bytes(pol);(target/'original_pol.tbl').write_bytes(sources['table'].read_bytes())
        texture=source/(prefix+'_tex.bin.nz')
        if not texture.exists():texture=source/(prefix+'_tex.bin')
        tex=models.read_payload(texture);(target/'original_texture_decoded.bin').write_bytes(tex)
        result=texture_bank.export_bank(source/(prefix+'_tex.tbl'),target/'original_texture_decoded.bin',target/'textures','twiddled')
        entry=dict(name=prefix,source_folder=folder,chunks=len(chunks),textures=len(result['textures']),polygon_sha256=hashlib.sha256(pol).hexdigest(),texture_sha256=hashlib.sha256(tex).hexdigest())
        (target/'manifest.json').write_text(json.dumps(entry,indent=2)+'\n');banks.append(entry)
    report=dict(source_image_sha256=SHA256,source_configure='0C0F88C0',source_loader='0C0F9A00',source_script_selector='0C0F5FE0-0C0F6122',rows=408,tokens=sum(len(r['tokens'])for r in records),bunta_data_sha256=hashlib.sha256(data).hexdigest(),element_table='0C319CC4',elements=elements,banks=banks,records=records)
    (out/'bunta-scripts.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({k:v for k,v in report.items()if k not in('records','elements')},indent=2))
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('root',type=Path)
    a=p.parse_args();export(a.image,a.hostfs,a.root)
