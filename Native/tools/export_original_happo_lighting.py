"""Import bounded040D60 original light-object/packet capture; not an emulator."""
import hashlib,json,re,sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
SHA='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    if hashlib.sha256(Path(sys.argv[1]).read_bytes()).hexdigest()!=SHA:raise ValueError('Wrong canonical image')
    records=[]
    initializers=[]
    for line in Path(sys.argv[2]).read_text().splitlines():
        m=re.match(r' initializer (\d+) 041ce0 words (.*)',line)
        if m:
            words=[int(w,16) for w in m[2].split()]
            if int(m[1])!=len(initializers) or words!=[0x3e4ccccd]*3+[0x0c380f44]:raise ValueError('Missing or invalid startup041CE0 vector')
            initializers.append(words)
        m=re.match(r'row (\d+) instructions (\d+)',line)
        if m:
            if int(m[1])!=len(records):raise ValueError('Nonsequential row')
            records.append({'row':int(m[1]),'instructions':int(m[2]),'objects':[],'packets':[],'cosines':{}})
        m=re.match(r' registeredObject (\d+) ownerOffset (\d+) words (.*)',line)
        if m:
            words=[int(w,16) for w in m[3].split()]
            if len(words)!=23 or int(m[1])!=len(records[-1]['objects']):raise ValueError('Invalid captured object')
            records[-1]['objects'].append({'offset':int(m[2]),'words':words})
        m=re.match(r' highGLM (.*)',line)
        if m:records[-1]['ambient']=[int(w,16) for w in m[1].split()][5:8]
        m=re.match(r' packed (\d+) (.*)',line)
        if m:records[-1]['packets'].append([int(w,16) for w in m[2].split()])
        m=re.match(r' cosine (\d+) (.*)',line)
        if m:records[-1]['cosines'][int(m[1])]=[int(w,16) for w in m[2].split()]
    if len(initializers)!=4:raise ValueError('Capture omitted source startup041CE0')
    if len(records)!=4 or [len(r['objects']) for r in records]!=[0,3,10,12]:raise ValueError('Unexpected Happo registration')
    lines=['#pragma once','#include <array>','#include <cstdint>',f'// Original041CE0 startup +040D60 source capture. Canonical SHA256 {SHA}',
           'namespace idas3::original {','struct OriginalHappoCapturedLight { unsigned ownerOffset; std::array<std::uint32_t,23> object; std::array<std::uint32_t,2> coefficients,cosines; };',
           'inline constexpr std::array<unsigned,4> originalHappoLightCounts{0,3,10,12};',
           'inline constexpr std::array<std::array<std::uint32_t,3>,4> originalHappoAmbientWords{{'+','.join('{{'+','.join(hex(w)+'u' for w in r['ambient'])+'}}' for r in records)+'}};',
           'inline constexpr std::array<std::array<OriginalHappoCapturedLight,16>,4> originalHappoCapturedLights{{']
    for r in records:
        lines.append(' {{')
        for i,o in enumerate(r['objects']):
            p=r['packets'][i];c=r['cosines'].get(i,[0,0])
            lines.append('  {'+str(o['offset'])+'u,{{'+','.join(hex(w)+'u' for w in o['words'])+'}},{{'+','.join(hex(w)+'u' for w in p[6:8])+'}},{{'+','.join(hex(w)+'u' for w in c)+'}}},')
        lines.append(' }},')
    lines+=['}};','} // namespace idas3::original','']
    (ROOT/'src/original_happo_lighting_data.h').write_text('\n'.join(lines))
    (ROOT/'verification/original-course-lighting/happo-records.json').write_text(json.dumps({'sha256':SHA,'initializer':'0C041CE0','initialized_vector_words':initializers[0],'records':records},indent=2)+'\n')
    print('Imported actual Happo041CE0 startup +040D60 light states for four conditions.')
if __name__=='__main__':main()
