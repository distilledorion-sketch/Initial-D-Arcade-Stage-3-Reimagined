"""Preserve original race effect banks, with source identity and cue tables."""
import argparse,hashlib,json,struct
from pathlib import Path

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--image',type=Path,required=True);p.add_argument('--project',type=Path,required=True)
    a=p.parse_args();program=a.image.read_bytes();base=0xc020000
    if hashlib.sha256(program).hexdigest()!='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335':
        raise ValueError('Canonical original program identity mismatch')
    u32=lambda at:struct.unpack_from('<I',program,at-base)[0]
    dest=a.project/'data/original_audio/race';dest.mkdir(parents=True,exist_ok=True);records=[]
    for bank in (2,3,4,5):
        entry=0xc31ed30+12*bank;address=u32(entry)
        name=program[address-base:program.index(0,address-base)].decode('ascii')
        src=a.hostfs/'sound/pack'/(name+'.nz');data=src.read_bytes()
        if data[:4]!=b'DTPK' or struct.unpack_from('<I',data,8)[0]!=len(data):raise ValueError('Invalid original DTPK')
        target=dest/(Path(name).stem+'.dtpk');target.write_bytes(data)
        table=u32(entry+4);cues=[]
        for cue in range(u32(entry+8)):
            command,level,reserved=struct.unpack_from('<III',program,table+12*cue-base)
            cues.append(dict(cue=cue,command=f'{command:08x}',level=level,reserved=reserved))
        records.append(dict(file=target.name,source=str(src.resolve()),sha256=hashlib.sha256(data).hexdigest(),bank=bank,cue_table=f'{table:08x}',cues=cues))
        if bank==3:
            (dest/'skid_cues.bin').write_bytes(b'IDSK0001'+struct.pack('<I',len(cues))+b''.join(struct.pack('<I',int(c['command'],16)) for c in cues))
    manifest=dict(schema='idas3-race-audio-v1',program_sha256=hashlib.sha256(program).hexdigest(),scene=4,
        scene_source='067A06 -> 1416A0, table31DE94 row4: bank2,3,3,4,5',
        scope='Preserved original banks and cue mappings, including PACK23 compound skid sequences. Import does not implement sequence playback or cabinet DSP/envelope/pan.',banks=records)
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Preserved',', '.join(r['file'] for r in records))
if __name__=='__main__':main()
