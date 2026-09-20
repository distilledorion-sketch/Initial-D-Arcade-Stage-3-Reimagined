"""Capture original A9 voice setup with bounded ARM calls in private RAM.

No driver runtime/IRQ or device is entered. Source instruction bytes are guarded
by the reusable Reference's SHA and memory protections. The importer preserves
raw AICA registers, voice flags and the selected velocity table, not synthesized
audio. Production uses the original PCM from the independently preserved banks.
"""
from pathlib import Path
import hashlib,json,struct,sys,os
sys.path.insert(0,str(Path(__file__).resolve().parents[2].parent/"InitialDRemake/tools"))
from reference_original_music_sequence import Reference,WORK,DRIVER_SHA,UC_HOOK_CODE,UC_ARM_REG_R7

ROOT=Path(__file__).resolve().parents[1]
DEST=ROOT/'data/original_audio/oneshot'
def fnv(b):
    n=14695981039346656037
    for v in b:n=((n^v)*1099511628211)&0xffffffffffffffff
    return n
def level(c):
    n=c[0]
    for gain in c[1:5]:
        n=n*(gain*2)>>8
        if n:n+=1
    if not c[6]&1:
        n=n*((c[5]&127)*2)>>8
        if n:n+=1
    n^=255
    return n if n==255 else n>>1
def main():
    DEST.mkdir(parents=True,exist_ok=True);report=dict(driverSha256=DRIVER_SHA,banks=[],comparisons=0)
    for number in (tuple(map(int,os.environ["IDAS3_ONESHOT_BANKS"].split(","))) if "IDAS3_ONESHOT_BANKS" in os.environ else (20,21,22,24,25)):
        name=f'PACK{number}';bank=(ROOT/'data/original_audio'/('menu' if number==21 else 'race')/(name+'.dtpk')).read_bytes()
        (WORK/(name+'_sound.bin')).write_bytes(bank)
        r=Reference(name);u=lambda p:struct.unpack_from('<I',bank,p)[0]
        count=struct.unpack_from('<H',bank,u(0x30)+16)[0]+1
        assert count<128
        # All used banks have fixed plain A9 PCM groups and no mutable pan,
        # pitch randomization or multilayer program state.
        r.run(0x7b64,0xa9000000|((number-20)<<16));r.run(0x7234);r.pending=[]
        tables={}
        def table(uc,pc,size,user):tables['value']=r.read(uc.reg_read(UC_ARM_REG_R7),128)
        r.cpu.hook_add(UC_HOOK_CODE,table,begin=0x4300,end=0x4300)
        rows=[];payload=bytearray()
        for playback in range(count):
            base=None;velocity=None
            for volume in range(1,128):
                captured=r.dispatch(0x9f000000|(playback<<16)|(volume<<8))
                assert len(captured)==1,(name,playback,volume,captured)
                row=captured[0];regs=row['registers'];record=bank[row['layer']:row['layer']+64]
                assert record[28]==0 and (record[34]&0x83) in (0,0x80)
                assert regs[0]&0x380==0 and not regs[0]&0x200,'Only verified nonlooping PCM16 SFX'
                assert level(row['volume'])==regs[10]>>8
                if base is None:base=row;velocity=tables['value']
                else:
                    assert tables['value']==velocity
                    assert regs[:10]+[regs[10]&255]+regs[11:]==base['registers'][:10]+[base['registers'][10]&255]+base['registers'][11:]
                    assert row['volume'][1:]==base['volume'][1:]
                report['comparisons']+=19
            # Default capture at the actual last velocity127.
            row=captured[0];regs=row['registers'];record=bank[row['layer']:row['layer']+64]
            # Every used A9 bank has neutral alternate offset64, channel gain
            # directly follows alternate bank volume. Prove all128 levels.
            volumeRows=[]
            for gain in range(128):
                r.run(0x4624,0xa0100000|(gain<<8))
                live=list(struct.unpack('<18I',r.read(row['address'],72)))
                context=list(row['volume']);context[3]=gain
                assert live[10]>>8==level(context),'Alternate gain must also update already active SFX'
                assert live[:10]+[live[10]&255]+live[11:]==regs[:10]+[regs[10]&255]+regs[11:]
                report['comparisons']+=18
                test=r.dispatch(0x9f000000|(playback<<16)|(127<<8))[0]
                assert test['volume'][3]==gain
                assert test['registers'][10]>>8==level(test['volume'])
                volumeRows.append(test['registers'][10]>>8);report['comparisons']+=2
            r.run(0x4624,0xa0107f00)
            payload+=struct.pack('<18I',*regs)
            payload+=bytes([row['voiceFlags0'],row['voiceFlags1'],record[35],(128+record[36])&255])
            payload+=struct.pack('<I',128+record[36])
            payload+=bytes(row['volume'])+b'\0'+velocity
            rows.append(dict(playback=playback,layer=row['layer'],registers=regs,flags=[row['voiceFlags0'],row['voiceFlags1']],priority=128+record[36],volume=row['volume'],velocityTable=velocity.hex(),gainTotalLevels=volumeRows))
        target=DEST/(name+'.idso')
        target.write_bytes(struct.pack('<4sIIIQ',b'IDSO',1,number,count,fnv(bank))+payload)
        report['banks'].append(dict(name=name,bankSha256=hashlib.sha256(bank).hexdigest(),assetSha256=hashlib.sha256(target.read_bytes()).hexdigest(),records=rows,calls=r.callCount))
        print(name,count,'source records captured',r.callCount,'bounded calls',flush=True)
    out=ROOT/('verification/original-oneshot-audio-'+os.environ['IDAS3_ONESHOT_BANKS'] if 'IDAS3_ONESHOT_BANKS' in os.environ else 'verification/original-oneshot-audio');out.mkdir(parents=True,exist_ok=True)
    (out/'source-voices.json').write_text(json.dumps(report,indent=2))
    print('PASS',report['comparisons'],'source register/volume comparisons')
if __name__=='__main__':main()

