"""Preserve continuous banks and extract the original engine control tables."""
import argparse, hashlib, json, struct
from pathlib import Path

IMAGE_SHA256 = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('hostfs', 'image', 'project'):
        parser.add_argument('--'+name, type=Path, required=True)
    args = parser.parse_args()
    image = args.image.read_bytes()
    if hashlib.sha256(image).hexdigest() != IMAGE_SHA256:
        raise ValueError('Canonical original program identity mismatch')
    base = 0x0c020000
    word = lambda a: struct.unpack_from('<I', image, a-base)[0]
    string = lambda a: image[a-base:image.index(0, a-base)].decode('ascii')
    dest = args.project/'data/original_audio/continuous'
    dest.mkdir(parents=True, exist_ok=True)
    driver=(args.hostfs/'binary/AICADRV.bin').read_bytes()
    driver_sha=hashlib.sha256(driver).hexdigest()
    if driver_sha!='6b1a51d7576b6333a0aa741ce3b0b3ca6c3818d6de2b092d37b9196672478f67':
        raise ValueError('Original AM2 sound driver identity mismatch')
    pitch=b''.join(struct.pack('<H',struct.unpack_from('<I',driver,0x80b4+4*i)[0]&0xffff) for i in range(384))
    (dest/'pitch_table.bin').write_bytes(pitch)
    voice=b'ICSV0001'+driver[0x339c:0x33bc]+driver[0x7f30:0x8030]
    (dest/'voice_tables.bin').write_bytes(voice)
    tables = bytearray(b'IDEC0001'+struct.pack('<I',36))
    families, bank_names = [], {'PACK10.bin'}
    selections = bytearray(b'IDES0001'+struct.pack('<I',36))
    for family in range(36):
        curve = word(0x0c2fba84+4*family)
        tables.extend(image[curve-base:curve-base+112])
        tables.extend(image[0x0c25cd08+24*family-base:0x0c25cd0c+24*family-base])
        descriptor = word(0x0c2fb3d4+family*4)
        groups = []
        for i in range(2):
            entry = descriptor+24*i
            name, parameters, count = string(word(entry)), word(entry+4), word(entry+8)
            if count != 1:
                raise ValueError('Unexpected source continuous voice count')
            bank_names.add(name)
            selections.extend(struct.pack('<III',int(Path(name).stem[4:]),word(parameters),word(parameters+4)))
            tables.extend(struct.pack('<I',word(parameters+20)))
            groups.append(dict(bank=name, instrument=word(parameters), parameter_address=f'{parameters:08x}',
                parameters=[word(parameters+j*4) for j in range(9)], descriptor_address=f'{entry:08x}'))
        families.append(dict(family=family, curve_address=f'{curve:08x}', groups=groups))
    for i in range(3):
        tables.extend(struct.pack('<I',struct.unpack_from('<H',image,0x0c2fbb14+2*i-base)[0]))
    (dest/'control.bin').write_bytes(tables)
    selections.extend(struct.pack('<II',10,word(0x0c25d37c)))
    (dest/'selections.bin').write_bytes(selections)
    banks=[]
    for name in sorted(bank_names):
        path=args.hostfs/'sound/pack'/(name+'.nz')
        data=path.read_bytes()
        if data[:4]!=b'DTPK' or struct.unpack_from('<I',data,8)[0]!=len(data):
            raise ValueError('Invalid original bank: '+name)
        if not struct.unpack_from('<I',data,0x34)[0]:
            raise ValueError('Continuous bank has no ICS table: '+name)
        target=Path(name).stem+'.dtpk'
        (dest/target).write_bytes(data)
        banks.append(dict(file=target, source=str(path.resolve()), bytes=len(data), sha256=hashlib.sha256(data).hexdigest()))
    manifest=dict(schema='idas3-continuous-audio-v1',program_sha256=IMAGE_SHA256,
        driver_sha256=driver_sha,pitch_table_sha256=hashlib.sha256(pitch).hexdigest(),
        voice_tables_sha256=hashlib.sha256(voice).hexdigest(),
        control_sha256=hashlib.sha256(tables).hexdigest(),
        selections_sha256=hashlib.sha256(selections).hexdigest(),
        source='0C3E20 initialization, 0C3B60 curves, 0C3C60 settings, 0C4360 frame controller',
        scope='Original control data and complete DTPK banks. Import does not implement ICS playback or cabinet DSP.',
        families=families,banks=banks)
    (dest/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Preserved {len(banks)} banks and {len(families)} continuous control families; control SHA256 {manifest["control_sha256"]}')

if __name__=='__main__':main()
