#pragma once
#include "sh4_scalar_reference.h"

namespace idas3::reference {
// Bind external helper calls, vtables and literal pools. Geometry, visibility
// decisions and object placement are still read from the selected program.
inline unsigned courseSymbol(const RefMemory& memory, unsigned english) {
    if (!memory.japanRevC) return english;
    switch (english) {
        case 0x0c03a320: case 0x0c03aa40: case 0x0c03d6e0:
        case 0x0c03d740: case 0x0c03d7e0: case 0x0c042520: return english - 0x280;
        case 0x0c05a8e0: return english - 0x340;
        case 0x0c085220: return english - 0x5a0;
        case 0x0c191cfc: return english + 0x13460;
        case 0x0c19ce80: case 0x0c19cec0: case 0x0c19cf40:
        case 0x0c19cfa0: case 0x0c19d000: case 0x0c19d0c0: return english + 0x13380;
        case 0x0c1d7120: case 0x0c1f65c0: case 0x0c1f6610: case 0x0c1f67e0:
        case 0x0c1f68a0: case 0x0c1f6950: case 0x0c1f6ac0: case 0x0c1f6af0:
        case 0x0c1fb360: case 0x0c1fbd60: case 0x0c1fbf80: case 0x0c1fc0a0:
        case 0x0c1fc5a0: case 0x0c1fd060: case 0x0c2223b8: return english + 0x7140;
        case 0x0c2a470c: case 0x0c2a4a40: return english + 0x7cb0;
        case 0x0c381ddc: case 0x0c38213c: case 0x0c38dc9c: case 0x0c38dd74:
        case 0x0c38de4c: case 0x0c38df24: case 0x0c38dffc: case 0x0c38e0d4:
        case 0x0c38e1ac: case 0x0c38e284: case 0x0c38e35c: case 0x0c38e434:
        case 0x0c38e50c: case 0x0c38e5e4: case 0x0c38e6bc: case 0x0c38e794:
            return english + 0x9440;
        default: throw std::runtime_error("Unknown course capture symbol: " + hex(english));
    }
}
}
