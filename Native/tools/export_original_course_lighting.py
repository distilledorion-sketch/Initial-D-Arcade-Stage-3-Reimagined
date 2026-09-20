"""Import exact authoring words and bounded-source spot coefficient captures.

Usage: python export_original_course_lighting.py CANONICAL_IMAGE CAPTURE_TEXT
Capture text is emitted by capture_original_course_lighting.cpp; this does not
launch any emulated platform or execute original hardware-facing code.
"""
import hashlib,json,re,struct,sys
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
SHA='efda831f1212db54cc2e4ba53424fe390f91b2daabb07e93c1e389c3736d0335'
def main():
    source=Path(sys.argv[1]).read_bytes()
    if hashlib.sha256(source).hexdigest()!=SHA: raise ValueError('Wrong canonical image')
    captures=[]
    for line in Path(sys.argv[2]).read_text().splitlines():
        m=re.match(r'row (\d+) instructions (\d+)',line)
        if m:
            if int(m[1])!=len(captures): raise ValueError('Nonsequential source rows')
            captures.append({'row':int(m[1]),'instructions':int(m[2]),'packets':[],'cosines':{}})
        m=re.match(r' cosine (\d+) (.*)',line)
        if m: captures[-1]['cosines'][int(m[1])]=[int(w,16) for w in m[2].split()]
        m=re.match(r' packed (\d+) (.*)',line)
        if m:
            p=[int(w,16) for w in m[2].split()]
            if len(p)!=8 or int(m[1])!=len(captures[-1]['packets']): raise ValueError('Bad source packet')
            captures[-1]['packets'].append(p)
    if len(captures)!=36: raise ValueError('Need all36 source rows')
    words=[];coefficients=[];cosines=[]
    for row,c in enumerate(captures):
        w=list(struct.unpack_from('<88I',source,0x33c864-0x20000+368*row))
        fixed,relative,spot=min(3,w[3]),min(2,w[22]),min(4,w[35])
        if len(c['packets'])!=fixed+relative+spot: raise ValueError('Original registration count mismatch')
        words.append(w);coefficients.append([p[6:8] for p in c['packets'][fixed+relative:]]+[[0,0]]*(4-spot))
        cosines.append([c['cosines'][i+fixed+relative] for i in range(spot)]+[[0,0]]*(4-spot))
        c['source_address']=0x0c33c864+368*row
        c['counts']={'parallel':fixed,'relative':relative,'spot':spot}
    out=['#pragma once','#include <array>','#include <cstdint>',
         '// Canonical33C864,36*368-byte rows. Authored offsets0..351.',f'// SHA256 {SHA}',
         '// Spot coefficient words captured from original1D3860/205DA0, revision0x10.',
         'namespace idas3::original {','inline constexpr std::array<std::array<std::uint32_t,88>,36> originalCourseLightingWords{{']
    out += ['    {{'+','.join(f'0x{x:08x}u' for x in w)+'}},' for w in words]
    out += ['}};','inline constexpr std::array<std::array<std::array<std::uint32_t,2>,4>,36> originalCourseSpotCoefficientWords{{']
    out += ['    {{'+','.join('{{'+','.join(f'0x{x:08x}u' for x in p)+'}}' for p in c)+'}},' for c in coefficients]
    out += ['}};','inline constexpr std::array<std::array<std::array<std::uint32_t,2>,4>,36> originalCourseSpotCosineWords{{']
    out += ['    {{'+','.join('{{'+','.join(f'0x{x:08x}u' for x in p)+'}}' for p in c)+'}},' for c in cosines]
    out += ['}};','inline constexpr std::array<std::array<std::uint32_t,3>,31> originalIrohazakaLightPositionWords{{']
    out += ['    {{'+','.join(hex(w)+'u' for w in struct.unpack_from('<3I',source,0x2a506c-0x20000+12*i))+'}},' for i in range(31)]
    out += ['}};','} // namespace idas3::original','']
    (ROOT/'src/original_course_lighting_data.h').write_text('\n'.join(out))
    dest=ROOT/'verification/original-course-lighting';dest.mkdir(parents=True,exist_ok=True)
    (dest/'source-records.json').write_text(json.dumps({'canonical_sha256':SHA,'rows':captures},indent=2)+'\n')
    print('Imported36 authored lighting rows and bounded-source spot coefficients.')
if __name__=='__main__': main()
