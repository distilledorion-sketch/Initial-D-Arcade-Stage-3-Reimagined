"""Preserve original A8 selection/result banks and decode their AICA samples.

This is an importer, not a MIDI renderer. Original program/group/layer and song
bytes stay in each DTPK file; the native parser resolves them at load time.
"""
import argparse, hashlib, json, struct
from pathlib import Path

EXPECTED = {
    'TYPE': ('c5620bd3afb65b6fa6f7ce73aa41ddbaeca4b306fb1b6a27dbf9507e584d1e69', 0x1a8, 109),
    'SELECT': ('043963567bcd2b130aec6582743df27ebb7d5d530735aa18e52b6b29fe084520', 0xa8, 103),
    'RESULT': ('5832f73859d86fc333235cf8d4e422807dfe0cb479dea3c36e646623c392065d', 0x1a8, 108),
}

def decode_sample(raw, encoding):
    if encoding == 0:
        if len(raw) % 2: raise ValueError('Unaligned PCM16')
        return list(struct.unpack('<'+'h'*(len(raw)//2), raw))
    if encoding == 1:
        return [n*256 for n in struct.unpack('<'+'b'*len(raw), raw)]
    if encoding != 2: raise ValueError('Stream ADPCM requires continuous history')
    quant, previous, result = 127, 0, []
    factors = (230, 230, 230, 230, 307, 409, 512, 614)
    for byte in raw:
        for nibble in (byte & 15, byte >> 4):
            index = nibble & 7
            delta = min(32767, quant*(2*index+1)//8)
            previous = max(-32768, min(32767, previous + (-delta if nibble & 8 else delta)))
            quant = max(127, min(24576, quant*factors[index]//256))
            result.append(previous)
    return result

def inventory(bank, name, destination):
    sha, command, level = EXPECTED[name]
    if hashlib.sha256(bank).hexdigest() != sha: raise ValueError(name+' bank identity mismatch')
    u32=lambda at:struct.unpack_from('<I',bank,at)[0]
    u16=lambda at:struct.unpack_from('<H',bank,at)[0]
    if bank[:4]!=b'DTPK' or u32(8)!=len(bank) or u32(0x30) or u32(0x34): raise ValueError('Not recovered A8 layout')
    samples=[];table=u32(0x3c);count=u32(table)+1
    dest=destination/name;dest.mkdir(parents=True,exist_ok=True)
    for i in range(count):
        at=table+4+16*i;location,lsa,lea,channels,length=struct.unpack_from('<IHHII',bank,at)
        offset=location&0x7fffff;encoding=(location>>23)&3;looping=bool(location&0x2000000)
        if location&0xfc000000 or channels or offset+length>len(bank):raise ValueError('Unknown sample format')
        raw=bank[offset:offset+length];pcm=decode_sample(raw,encoding)
        if not 0<=lsa<lea<len(pcm):raise ValueError('Invalid sample loop bounds')
        data=struct.pack('<'+'h'*len(pcm),*pcm);(dest/f'{i:02d}.pcm16').write_bytes(data)
        samples.append(dict(id=i,source_descriptor=list(struct.unpack_from('<4I',bank,at)),
            source_offset=offset,source_bytes=length,encoding=encoding,looping=looping,
            loop_start=lsa,loop_end_exclusive=lea,decoded_frames=len(pcm),
            pcm_file=f'{name}/{i:02d}.pcm16',pcm_sha256=hashlib.sha256(data).hexdigest()))
    program=u32(0x24);sub=program+u16(program+2)
    if u16(program)!=0:raise ValueError('Multiple instrument subbanks')
    programs=[sub+u16(sub+2+2*i) for i in range(u16(sub)+1)]
    seq=u32(0x2c);group=seq+u16(seq+4)
    if u32(seq)!=0 or u32(group)!=0 or bank[seq+7]!=0xa8:raise ValueError('Unknown A8 group')
    actual_command=bank[seq+7]|bank[seq+6]<<8
    if command!=actual_command:raise ValueError('Source cue bank changed')
    song=seq+u32(group+4)
    (destination/(name+'.dtpk')).write_bytes(bank)
    return dict(name=name,sha256=sha,bytes=len(bank),source_command=command,source_level=level,
        program_table=program,program_offsets=programs,sequence_table=seq,song_offset=song,
        song_bytes=u32(0x38)-song,sample_table=table,samples=samples)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--hostfs',type=Path,required=True);p.add_argument('--project',type=Path,required=True)
    a=p.parse_args();dest=a.project/'data/original_audio/selection';dest.mkdir(parents=True,exist_ok=True)
    driver=(a.hostfs/'binary/AICADRV.bin').read_bytes();driver_sha=hashlib.sha256(driver).hexdigest()
    if driver_sha!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':raise ValueError('Original driver identity mismatch')
    descriptor=driver[0x86cc:0x86dc]
    if struct.unpack('<4I',descriptor)!=(0x0200916c,0x00640000,0,200):raise ValueError('Built-in sample1 changed')
    (dest/'builtin_samples.bin').write_bytes(b'MUSB0001'+struct.pack('<II',1,1)+descriptor+driver[0x916c:0x9234])
    records=[]
    for name in EXPECTED:
        source=a.hostfs/'sound/pack'/(name+'.bin.nz')
        # The canonical .nz files here already begin DTPK; do not re-decompress.
        records.append(inventory(source.read_bytes(),name,dest))
    manifest=dict(schema='idas3-original-selection-music-v1',banks=records,
        builtin_sample=dict(source_id=1,driver_sha256=driver_sha,descriptor_offset=0x86cc,sample_offset=0x916c,bytes=200,encoding=0,loop_start=0,loop_end_exclusive=100,pcm_sha256=hashlib.sha256(driver[0x916c:0x9234]).hexdigest()),
        pcm_format='Signed little-endian PCM16 at AICA native44100Hz before per-note pitch.',
        decoder='AICA PCMS0/1/2. Yamaha PCMS2 has no SPSD leakage; low nibble first; delta saturated before accumulation. PCMS2 saves pre-LSA quantizer/history and restores it for each loop. Original encoded bytes are retained in DTPK.',
        source_derivations=['Original ARM3084..3110 program lookup','Original ARM4494..4578 sample descriptor and offset lookup','Original ARM39A0..3AB4 layer selection','Original ARM3BEC..3C10 raw envelope/LFO transfer'],
        limitations='Bank import alone does not schedule or play the A8 songs. DSP/effects data remains original and requires playback support.')
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Preserved {len(records)} A8 banks, {sum(len(x["program_offsets"]) for x in records)} programs and {sum(len(x["samples"]) for x in records)} original samples')

if __name__=='__main__':main()
