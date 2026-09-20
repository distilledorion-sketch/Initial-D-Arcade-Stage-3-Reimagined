"""Preserve original DTPK DSP presets; never runs an emulator or audio device."""
from pathlib import Path
import argparse,hashlib,json,struct

DRIVER_SHA='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67'

def inspect(bank):
    word=lambda at:struct.unpack_from('<I',bank,at)[0]
    half=lambda at:struct.unpack_from('<H',bank,at)[0]
    if bank[:4]!=b'DTPK' or word(8)!=len(bank):raise ValueError('Invalid DTPK identity/size')
    if bank[19]>3:raise ValueError('Unknown ring code')
    offset=word(0x38);presets=[]
    if offset:
        count=word(offset)+1
        if not 0<count<=128 or offset+4+count*0xc24>len(bank):raise ValueError('DSP table extent')
        for index in range(count):
            at=offset+4+index*0xc24;raw=bank[at:at+0xc24]
            words=struct.unpack('<768I',raw[36:]);ops=[]
            if any(v&0xffff0000 for v in words) or any(words[192:256]):raise ValueError('Unknown DSP padding/high bits')
            for i in range(128):
                x,y,z,w=words[256+4*i:260+4*i]
                ops.append(dict(step=i,words=[x,y,z,w],IRA=(y>>7)&63,IWT=bool(y&64),IWA=(y>>1)&31,XSEL=bool(y&0x8000),
                    NOFL=bool(w&0x8000),TABLE=bool(z&0x8000),MRD=bool(z&0x2000),MWT=bool(z&0x4000),ADRL=bool(z&0x80),
                    SHIFT=(z>>4)&3,MASA=(w>>9)&63,ADREB=bool(w&256),NXADR=bool(w&128)))
            # All authored memory operands are fixed offsets. Test both the
            # declared ring and the persistent32K-word startup-ring case.
            aliases=[]
            for ringCode in sorted(set((2,bank[19]))):
                mask=(8192<<ringCode)-1
                for old in ops:
                    if not old['MWT']:continue
                    for new in ops:
                        if new['MRD'] and new['step']>old['step']:
                            wa=(words[128+old['MASA']]+old['NXADR'])&mask;ra=(words[128+new['MASA']]+new['NXADR'])&mask
                            if wa==ra:aliases.append(dict(ringCode=ringCode,writeStep=old['step'],readStep=new['step'],address=wa))
            presets.append(dict(index=index,sourceOffset=at,record=raw,recordSha256=hashlib.sha256(raw).hexdigest(),
                routes=list(struct.unpack('<16H',raw[:32])),unusedTrailingRoutes=list(struct.unpack('<2H',raw[32:36])),
                words=words,instructions=ops,sameSampleWriteReadAliases=aliases))
    at=word(0x20);count=bank[at];scenes=[]
    for index in range(count):
        relative=half(at+4+index*2)
        if not relative:scenes.append(dict(index=index,sourceOffset=0,bankId=255,preset=255));continue
        location=at+relative
        scenes.append(dict(index=index,sourceOffset=location,bankId=bank[location+3],preset=bank[location+4]))
    return dict(bankId=bank[18],declaredRingCode=bank[19],presets=presets,scenes=scenes)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True);a=p.parse_args()
    driver=(a.hostfs/'binary/AICADRV.bin').read_bytes()
    if hashlib.sha256(driver).hexdigest()!=DRIVER_SHA:raise ValueError('Driver identity mismatch')
    dest=a.project/'data/original_audio/dsp';dest.mkdir(parents=True,exist_ok=True);banks=[]
    for file in sorted((a.hostfs/'sound/pack').glob('*.bin.nz')):
        original=file.read_bytes();b=inspect(original);name=file.name[:-7]
        out=bytearray(b'IDSP0001'+struct.pack('<4I',b['bankId'],b['declaredRingCode'],len(b['presets']),len(b['scenes'])))
        for row in b['presets']:out+=struct.pack('<I',row['sourceOffset'])+row['record']
        for row in b['scenes']:out+=struct.pack('<3I',row['sourceOffset'],row['bankId'],row['preset'])
        (dest/(name+'.idsp')).write_bytes(out)
        for row in b['presets']:del row['record'];del row['words']
        banks.append(dict(name=name,source=str(file.resolve()),sourceSha256=hashlib.sha256(original).hexdigest(),assetSha256=hashlib.sha256(out).hexdigest(),**b))
    manifest=dict(schema='idas3-original-audio-dsp-v1',driverSha256=DRIVER_SHA,banks=banks,
        derivation=['ARM3124 resolvesbankheader12 andDSPtable38','ARM5F78 selectsbank/preset andresets','ARM61A4 copiesrecord24..C23 toregister3000..3BFF','ARM61F4 copies16effectroutes','ARM1230 latchesfirstbankringcode13','ARM18B0 ringtable18E4 and5F28 fills6000'],
        boundary='Only original bank bytes are read. Config lookup/cache and register writes have separate bounded ARM fixtures. Physical sound-chip processing and exact poll timing are not asserted.')
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Preserved',len(banks),'original bank DSP descriptors,',sum(len(b['presets']) for b in banks),'presets and',sum(len(b['scenes']) for b in banks),'scene selectors')

if __name__=='__main__':main()
