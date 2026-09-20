"""Extract isolated, unchanged AICA numerical methods for native voice tests.

Generated code is a test reference only. It contains no game/CPU startup,
interrupts, audio device, filesystem emulator, graphics or mapped hardware.
"""
import argparse,hashlib,json
from pathlib import Path

def braced(text,marker):
    start=text.index(marker);brace=text.index('{',start);depth=1;at=brace+1
    while depth:
        if text[at]=='{':depth+=1
        elif text[at]=='}':depth-=1
        at+=1
    return text[start:at]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    raw=a.source.read_bytes();s=raw.decode().replace(chr(13), "");h=a.source.with_suffix('.h').read_text()
    pre='''#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace music_hardware_reference {
using u8=std::uint8_t;using u16=std::uint16_t;using u32=std::uint32_t;using u64=std::uint64_t;
using s8=std::int8_t;using s16=std::int16_t;using s32=std::int32_t;using s64=std::int64_t;
using SampleType=s32;
// Preserve LP64 lround range on Windows, where long is only32bits. Slowest
// AEG rates exceed signed32bits before their intended u32 conversion.
inline std::int64_t lround(double value){return std::llround(value);}
#define key_printf(...)
#define aeg_printf(...)
#define feg_printf(...)
#define step_printf(...)
#define clip_verify(...)
constexpr u32 ARAM_MASK=0x1fffff;
static std::array<u8,ARAM_MASK+1> aica_ram{};
'''
    # Keep the original license notice in this generated test reference.
    out=[s[:s.index('#include')],pre,braced(h,'union fp_22_10')+';\n']
    out.append(s[s.index('static s32 volume_lut'):s.index('static void VolumePan')])
    out.append('#pragma pack(push,1)\n'+braced(s,'struct ChannelCommonData')+';\n#pragma pack(pop)\n')
    for marker in ('enum EGState','enum class LFOType','enum PCMSType'):out.append(braced(s,marker)+';\n')
    prefix=s[s.index('struct ChannelEx\n'):s.index('\tvoid Init(int cn')]
    out.append(prefix)
    methods=('void disable()','void enable()','SampleType InterpolateSample()','SampleType lowPassFilter(SampleType sample)',
        'bool Step(SampleType& oLeft','void SetAegState(EGState newstate)','void SetFegState(EGState newstate)',
        'void KEY_ON()','void KEY_OFF()','void UpdateStreamStep()','void UpdateSA()','void UpdateLoop()',
        'u32 EG_EffRate(u32 rate)','void UpdateAEG()','void UpdatePitch()','void UpdateLFO(bool derivedState)',
        'void UpdateAtts()','void UpdateFEG()')
    for m in methods:out.append(braced(s,m)+'\n')
    out.append('};\n')
    start=s.index('void (ChannelEx::*ChannelEx::STREAM_STEP_LUT')
    end=s.index('static OnLoad staticInit')
    out.append(s[start:end])
    out.append('}\n#undef key_printf\n#undef aeg_printf\n#undef feg_printf\n#undef step_printf\n#undef clip_verify\n')
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(''.join(out))
    report=dict(source=str(a.source.resolve()),sha256=hashlib.sha256(raw).hexdigest(),methods=methods,
        generated_sha256=hashlib.sha256(a.output.read_bytes()).hexdigest(),
        modifications='Source method bodies unchanged. Minimal integer types, logging no-ops and isolated2MiB byte array supply dependencies. A namespace lround facade preserves LP64 integer range on Windows for slowest AEG rates. Test initializes only numerical tables; no original runtime/CPU/device exists.')
    a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
if __name__=='__main__':main()

