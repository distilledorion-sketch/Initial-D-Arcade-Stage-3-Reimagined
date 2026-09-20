"""Read-only canonical audio reference inventory; never executes game code."""
from pathlib import Path
import argparse, hashlib, json, struct

EXPECTED_SHA = 'efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
REFERENCES = {
    0x0c1ed6a0: [0x0c0c4058,0x0c0c418c,0x0c0c4248,0x0c0c4884,0x0c0c4a2c,0x0c0c4bfc],
    0x0c1ed840: [0x0c0c405c,0x0c0c41a0],
    0x0c142860: [0x0c157ea4],
    0x0c0c4360: [0x0c0c4c68,0x0c142858,0x0c153454],
}
DESCRIPTIONS = {
    0x0c1ed6a0: 'Unpacked continuous A5/A6 commands: literals belong to player engine init/reset/update.',
    0x0c1ed840: 'Unpacked A4 effect control: literals belong to player engine initialization.',
    0x0c142860: 'Single normal contact-completion wrapper; caller is player157AE0.',
    0x0c0c4360: 'Player control entry: normal142800, zero-throttle suffix0C4C4A, debug153384.',
}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('canonical_image',type=Path);parser.add_argument('output',type=Path)
    args=parser.parse_args();image=args.canonical_image.read_bytes()
    digest=hashlib.sha256(image).hexdigest()
    if digest!=EXPECTED_SHA: raise ValueError('Unexpected canonical image identity')
    rows=[]
    for target,expected in REFERENCES.items():
        needle=struct.pack('<I',target);found=[];position=image.find(needle)
        while position>=0:
            found.append(position+0x0c020000);position=image.find(needle,position+1)
        if found!=expected: raise ValueError(f'Changed canonical references for {target:08X}')
        rows.append(dict(target=f'{target:08X}',literalAddresses=[f'{a:08X}' for a in found],description=DESCRIPTIONS[target]))
    report=dict(sourceSha256=digest,sourceBytes=len(image),references=rows,
        scope='Complete byte-pattern inventory for four known continuous-sound entry pointers; interpretation cross-checked against containing source control flow. This is not proof that every possible dynamic sound producer is absent.',
        result='No separate original rival engine initialization, RPM controller, distance attenuation or pan path was identified in this bounded investigation. Do not synthesize a second player engine on this evidence.')
    args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_text(json.dumps(report,indent=2)+'\n')
    print(f'Canonical continuous audio reference inventory passed: {len(image)} bytes, {len(rows)} targets, {sum(len(r["literalAddresses"]) for r in rows)} exact references; no runtime execution.')

if __name__=='__main__':main()
