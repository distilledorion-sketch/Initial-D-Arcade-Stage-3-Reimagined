"""Preserve the owner's original SPSD streams with identity and header metadata."""
import argparse, hashlib, json, shutil, struct
from pathlib import Path

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hostfs',type=Path,required=True)
    parser.add_argument('--project',type=Path,required=True)
    args=parser.parse_args()
    destination=args.project/'data/original_audio/streams'
    destination.mkdir(parents=True,exist_ok=True)
    records=[]
    for source in sorted((args.hostfs/'sound/stream').glob('*.bin')):
        data=source.read_bytes()
        if data[:4]!=b'SPSD':raise ValueError(f'Unidentified original stream {source}')
        codec,flags,index,size=struct.unpack_from('<BBHI',data,8)
        channels=2 if flags&3 else 1
        rate=struct.unpack_from('<H',data,42)[0]
        if codec not in (0,1,3) or size>len(data)-64:raise ValueError('Invalid SPSD bounds')
        frames=size//channels*(2 if codec==3 else 1)//(2 if codec==0 else 1)
        target=destination/source.name
        shutil.copyfile(source,target)
        records.append(dict(file=source.name,source=str(source.resolve()),sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),codec=codec,channels=channels,sample_rate=rate,frames=frames,seconds=frames/rate,looping=bool(flags&128),interleave=index))
    manifest=dict(schema='idas3-original-spsd-v1',transformation='none; source bytes preserved',decoder_reference='https://github.com/vgmstream/vgmstream/blob/master/src/meta/spsd.c',streams=records)
    (destination.parent/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(dict(streams=len(records),bytes=sum(r['bytes'] for r in records),seconds=sum(r['seconds'] for r in records))))
if __name__=='__main__':main()
