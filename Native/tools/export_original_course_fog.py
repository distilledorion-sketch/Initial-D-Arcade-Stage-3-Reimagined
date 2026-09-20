"""Export only authored fog words from canonical course environment records."""
from pathlib import Path
import hashlib,json,struct,sys

SOURCE_SHA='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    if len(sys.argv)!=3:raise SystemExit('canonical_image project_root required')
    image=Path(sys.argv[1]).read_bytes();root=Path(sys.argv[2])
    if hashlib.sha256(image).hexdigest()!=SOURCE_SHA:raise ValueError('Canonical source hash mismatch')
    rows=[]
    for row in range(36):
        at=0x33c864-0x020000+row*368+352
        words=struct.unpack_from('<III',image,at)
        density=struct.unpack('<f',struct.pack('<I',words[0]))[0]
        maximum=struct.unpack('<f',struct.pack('<I',words[2]))[0]
        if not density>0 or not 0<=maximum<=1 or words[1]>0xffffff:raise ValueError('Fog source record bounds')
        rows.append(words)
    header=['#pragma once','#include <array>','#include <cstdint>',
        '// Generated from canonical33C864,36 rows x368 bytes; offsets352/356/360.',
        '// SHA256 '+SOURCE_SHA,'namespace idas3::original {',
        'inline constexpr std::array<std::array<std::uint32_t,3>,36> originalCourseFogWords{{']
    header.extend('    {{'+','.join(f'0x{x:08x}u' for x in row)+'}},' for row in rows)
    header.extend(['}};','}'])
    (root/'src/original_course_fog_data.h').write_text('\n'.join(header)+'\n')
    report={'source_sha256':SOURCE_SHA,'table':'0C33C864','row_stride':368,
        'index':'course*4+night*2+wet','word_offsets':[352,356,360],
        'rows':[{'row':i,'course':i//4,'night':bool(i&2),'wet':bool(i&1),
                 'raw_words':[f'{x:08X}' for x in row]} for i,row in enumerate(rows)]}
    target=root/'verification/original-course-fog';target.mkdir(parents=True,exist_ok=True)
    (target/'source-records.json').write_text(json.dumps(report,indent=2)+'\n')
    print('Exported36 canonical fog records; all source words and hash preserved.')
if __name__=='__main__':main()
