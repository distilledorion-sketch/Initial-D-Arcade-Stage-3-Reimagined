"""Extract the original menu PCM waveforms and preserve the source DTPK bank."""
import argparse, hashlib, json, struct, wave
from pathlib import Path

def export(bank: bytes, program: bytes, dest: Path):
    u32=lambda b,p:struct.unpack_from('<I',b,p)[0]
    assert bank[:4]==b'DTPK' and u32(bank,8)==len(bank)
    base=0x0c020000
    table=u32(program,0x0c143580-base)
    cue_table=u32(program,table+12+4-base)
    count=u32(program,table+12+8-base)
    assert table==0x0c31ed30 and cue_table==0x0c31eb68 and count==15
    seq,spd,samples=(u32(bank,p) for p in (0x2c,0x30,0x3c))
    dest.mkdir(parents=True,exist_ok=True)
    (dest/'PACK21.dtpk').write_bytes(bank)
    records=[]
    names={2:'change',3:'confirm',8:'back'}
    for cue in range(count):
        command,level,reserved=struct.unpack_from('<III',program,cue_table+12*cue-base)
        matches=[]
        for group in range(u32(bank,seq)+1):
            offset,bank_id,kind=struct.unpack_from('<HBB',bank,seq+4+4*group)
            if command&65535==(bank_id<<8)|kind:matches.append(seq+offset)
        assert len(matches)==1
        group=matches[0]; track=command>>16
        assert track<=u32(bank,group)
        p=seq+u32(bank,group+4+4*track); composition=bank[p:p+6]
        assert composition[:2]==b'\xc0\xdf' and composition[4]&0xf0==0x80 and composition[5]==255
        playback=composition[2]|((composition[4]&15)<<7)
        record=bank[spd+0x50+64*playback:spd+0x50+64*(playback+1)]
        assert len(record)==64 and record[0]==playback
        sample=record[2]
        location,loop_start,loop_end,channels,length=struct.unpack_from('<IHHII',bank,samples+4+sample*16)
        assert location&0xff800000==0 and channels==0 and length%2==0 and location+length<=len(bank)
        rate={0:44100,0xf400:22050,0xe800:11025}[int.from_bytes(record[10:12],'big')]
        pcm=bank[location:location+length]
        name=f'{cue:02d}_{names.get(cue,"unclassified")}.wav'
        with wave.open(str(dest/name),'wb') as out:
            out.setnchannels(1);out.setsampwidth(2);out.setframerate(rate);out.writeframes(pcm)
        records.append(dict(cue=cue,command=f'{command:08x}',level_word=level,reserved_word=reserved,
            semantic=names.get(cue),file=name,sequence_volume=composition[3],playback_id=playback,sample_id=sample,
            playback_record=record.hex(),source_offset=location,source_length=length,sample_rate=rate,channels=1,
            frames=length//2,loop_start_raw=loop_start,loop_end_raw=loop_end,pcm_sha256=hashlib.sha256(pcm).hexdigest()))
    return dict(schema='idas3-menu-dtpk-v1',bank_sha256=hashlib.sha256(bank).hexdigest(),
        program_sha256=hashlib.sha256(program).hexdigest(),bank_index=1,bank_table=f'{table:08x}',cue_table=f'{cue_table:08x}',
        decoder_scope='Exact source PCM16 mono waveform and rate; sound-driver envelope, gain, DSP sends and mix are not rendered into WAV.',
        cues=records)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs',type=Path,required=True);parser.add_argument('--image',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True);args=parser.parse_args()
    source=args.hostfs/'sound/pack/PACK21.bin.nz'
    if not source.exists():source=args.hostfs/'sound/pack/PACK21.bin'
    dest=args.project/'data/original_audio/menu'
    result=export(source.read_bytes(),args.image.read_bytes(),dest)
    result['source']=str(source.resolve())
    (dest/'manifest.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f"Preserved PACK21 and exported {len(result['cues'])} original PCM cues")
if __name__=='__main__':main()
