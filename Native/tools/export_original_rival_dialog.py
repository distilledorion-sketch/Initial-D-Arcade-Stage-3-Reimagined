"""Import the source-named rival scene artwork; preserve original model materials.

Run capture_original_rival_dialog_data first. The capture executes only ten
unchanged startup data constructors, not the original game or device runtime.
This importer does not guess scene draw matrices or flatten portraits to PNGs.
"""
from pathlib import Path
import argparse, hashlib, json, struct
from extract_original_menus import ORIGINAL_IMAGE_SHA256
import extract_original_models as models
import texture_bank

def digest(b):return hashlib.sha256(b).hexdigest()

def export(image, hostfs, root, art=True):
    raw=image.read_bytes()
    if digest(raw)!=ORIGINAL_IMAGE_SHA256:raise ValueError('Canonical source image required')
    def u32(a):return struct.unpack_from('<I',raw,a-0x0c020000)[0]
    def text(a):return raw[a-0x0c020000:].split(b'\0',1)[0].decode('ascii')
    expected={0x0c263950:'/driveA/model/rival_scene/rival_scene_pol',
        0x0c26397c:'/driveA/model/rival_scene/rival_scene_tex',
        0x0c2639a8:'/driveA/model/rival/r_%s/rival_%s_',
        0x0c263a14:'/driveA/model/rival_bg/bg%02d/rival_bg%02d%c%c_',
        0x0c263a44:'/driveA/model/rival_bg/bg08/rival_bg08_',
        0x0c263aac:'/driveA/font/namekana/namekana_spr',
        0x0c263ad0:'/driveA/font/namekana/namekana_font',
        0x0c263460:'/driveA/font/alphabet/alphabet_spr',
        0x0c263484:'/driveA/font/alphabet/alphabet_font'}
    for a,s in expected.items():
        if text(a)!=s:raise ValueError(f'Source resource string changed at {a:x}')
    out=root/'data/original_assets/rival_dialog';out.mkdir(parents=True,exist_ok=True)
    data=(out/'dialog.idasdialog').read_bytes();p=20
    def read32():
        nonlocal p
        v=struct.unpack_from('<I',data,p)[0];p+=4;return v
    def readstr():
        nonlocal p
        n=read32();v=data[p:p+n];p+=n;return v
    if data[:20]!=b'IDAS3RD1'+struct.pack('<3I',1,31,24):raise ValueError('Original captured dialogue header')
    identities=[]
    for enemy in range(31):
        character,course,night=read32(),read32(),read32();portrait=readstr().decode('ascii')
        if (character,course,night,portrait)!=(u32(0x0c26b81c+4*enemy),u32(0x0c2637f8+4*enemy),u32(0x0c263874+4*enemy),text(u32(0x0c31abe4+4*enemy))):raise ValueError('Captured identity mismatch')
        identities.append(dict(enemy=enemy,character=character,background_course=course,background_night=night,portrait=portrait))
    records=[];opcodes={};token_count=0
    for char in range(31):
        for kind in range(24):
            address,script=read32(),read32();words=[read32()for _ in range(7)];count=read32();tokens=[]
            for _ in range(count):
                a=read32();token=readstr()
                if raw[a-0x0c020000:a-0x0c020000+len(token)+1]!=token+b'\0':raise ValueError('Captured token bytes differ from source')
                tokens.append(dict(source_address=f'{a:08X}',bytes=token.decode('ascii')))
                opcodes[chr(token[0])]=opcodes.get(chr(token[0]),0)+1
            token_count+=count
            records.append(dict(character=char,kind=kind,source_record=f'{address:08X}',source_script=f'{script:08X}',layout_words=[f'{v:08X}'for v in words],tokens=tokens))
    if p!=len(data):raise ValueError('Trailing captured dialogue bytes')
    (out/'scripts.json').write_text(json.dumps(dict(identities=identities,records=records),indent=2)+'\n',encoding='utf-8')
    banks=[]
    if art:
        jobs={('rival_scene','rival_scene'),('continue','continue'),('v3sK17continue','v3sK17continue')}
        for id in identities:
            name=id['portrait'];jobs.add((f'rival/r_{name}',f'rival_{name}'))
            for wet in [0,1]:
                course=id['background_course'];night=id['background_night'];name=f'rival_bg{course:02d}'+(''if course==8 else('n'if night else'd')+('r'if wet else'f'))
                jobs.add((f'rival_bg/bg{course:02d}',name))
        #0F8B00 also has an optional cheering spectator for character29.
        # Preserve only files present in the source set; selection/variant6
        # handling must be closed before the native controller can request it.
        for i in range(6):jobs.add((f'rival/rf{i:02d}',f'rival_f{i:02d}'))
        for folder,prefix in sorted(jobs):
            source=hostfs/'model'/folder;target=out/'banks'/prefix;target.mkdir(parents=True,exist_ok=True)
            name,chunks,sources,pol=models.parse_model(source,prefix+'_pol.tbl',prefix+'_tex.tbl')
            models.write_binary(target/(prefix+'.idasmesh'),chunks)
            (target/'original_pol.bin').write_bytes(pol);(target/'original_pol.tbl').write_bytes(sources['table'].read_bytes())
            texture_path=source/(prefix+'_tex.bin.nz')
            if not texture_path.exists():texture_path=source/(prefix+'_tex.bin')
            texraw=models.read_payload(texture_path);(target/'original_texture_decoded.bin').write_bytes(texraw)
            result=texture_bank.export_bank(source/(prefix+'_tex.tbl'),target/'original_texture_decoded.bin',target/'textures','twiddled')
            entry=dict(name=prefix,source_folder=folder,chunks=len(chunks),textures=len(result['textures']),polygon_sha256=digest(pol),texture_sha256=digest(texraw),material_words_preserved=True)
            (target/'manifest.json').write_text(json.dumps(entry,indent=2)+'\n');banks.append(entry);print(prefix,len(chunks),len(result['textures']),flush=True)
        for name in ['alphabet','namekana']:
            source=hostfs/'font'/name;target=out/'fonts'/name
            result=texture_bank.export_bank(source/(name+'_spr.tbl'),source/(name+'_spr.bin.nz'),target/'textures','twiddled')
            # The _font bank is text-layout metadata consumed by0C5420, not
            # an independently chosen desktop font. Keep both table/payload.
            for suffix in ['_font.tbl','_font.bin.nz']:
                path=source/(name+suffix)
                if path.exists():(target/path.name).write_bytes(path.read_bytes())
            banks.append(dict(name=name,font=True,textures=len(result['textures'])))
    report=dict(schema='original-rival-dialog-recovery-v1',source_image_sha256=ORIGINAL_IMAGE_SHA256,
        source_owner='0F8340/0F8780/0F8B00',source_script_owner='0F5F40/0F69E0/0F6D40',
        private_original_assets=True,dialog_data_sha256=digest(data),row_count=len(records),token_count=token_count,
        command_initials=opcodes,banks=banks,ready_for_game=False,
        remaining=['Native script command interpreter and conditional profile branches','Original text reveal, portrait movement and background draw transforms','Shared RNG consumption and optional spectator6 resource branch','Full source choice draw and fade integration'])
    (out/'manifest.json').write_text(json.dumps(report,indent=2)+'\n');return report

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('image',type=Path);p.add_argument('hostfs',type=Path);p.add_argument('root',type=Path);p.add_argument('--scripts-only',action='store_true')
    a=p.parse_args();r=export(a.image,a.hostfs,a.root,not a.scripts_only);print(f"Saved{r['row_count']}rows/{r['token_count']}tokens/{len(r['banks'])}banks")
