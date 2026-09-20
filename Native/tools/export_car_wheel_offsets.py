"""Export static wheel-fit data referenced by the original191460 initializer."""
import argparse,hashlib,struct
from pathlib import Path
from export_original_assembly import IMAGE_HASH

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--image',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
    image=a.image.read_bytes()
    if hashlib.sha256(image).hexdigest()!=IMAGE_HASH:raise ValueError('Canonical image identity')
    def u16(address):return struct.unpack_from('<H',image,address-0xc020000)[0]
    def u32(address):return struct.unpack_from('<I',image,address-0xc020000)[0]
    #191470..19154C is a straight-line sequence of35 literals/stores.
    # Some model families share the pointer already published in33B230..24C.
    pointers=[];value=None
    for pc in range(0xc191470,0xc19154e,2):
        op=u16(pc)
        if op>>8==0xd2:value=u32(((pc+4)&~3)+(op&255)*4)
        elif op==0x6222:value=u32(value)
        elif op in (0x2122,0x1121):
            if value is None:raise ValueError('Uninitialized wheel-fit literal')
            pointers.append(value);value=None
        elif op!=0x7104:raise ValueError(f'Unexpected initializer opcode at{pc:08x}')
    if len(pointers)!=35:raise ValueError('Wheel-fit car count')
    lines=['#pragma once','#include <array>','#include <cstdint>','namespace idas3::original {',
        '// Authored191460 pointer initialization, then1913A0/1913E0 pair lookup.',
        '// Kept as F32 words; source image SHA256 '+IMAGE_HASH+'.',
        'inline constexpr std::array<std::array<std::array<std::uint32_t,2>,5>,35> originalCarWheelOffsetWords={{']
    for pointer in pointers:
        values=[f'{{0x{u32(pointer+n*8):08x}u,0x{u32(pointer+n*8+4):08x}u}}' for n in range(5)]
        lines.append('    {{'+','.join(values)+'}},')
    lines+=['}};','}'];a.out.write_text('\n'.join(lines)+'\n');print('Exported35 original wheel-fit tables')
if __name__=='__main__':main()
