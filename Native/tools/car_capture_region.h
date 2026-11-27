#pragma once
#include "sh4_scalar_reference.h"

namespace idas3::reference {
// Symbols at the boundaries of the original car routines. The CPU follows the
// selected program's own branches and literal pools inside those boundaries.
inline unsigned carSymbol(const RefMemory& memory, unsigned english) {
    if (!memory.japanRevC) return english;
    switch (english) {
        case 0x0c33b250: return 0x0c346c28; // semantic slot map pointers
        case 0x0c31c99c: return 0x0c328220; // active player profile
        case 0x0c035f00: return 0x0c035c80; // apply appearance preset
        case 0x0c055d60: return 0x0c055a20; // diagnostic
        case 0x0c0630b4: case 0x0c06316e: return english - 0x320;
        case 0x0c134a60: return english + 0x13360;
        case 0x0c16d680: return english + 0x13300;
        case 0x0c025fe0: case 0x0c026040: case 0x0c026160:
        case 0x0c026436: case 0x0c0264bc: case 0x0c0267c0:
        case 0x0c026d80: case 0x0c029040: case 0x0c02988e:
        case 0x0c029ad0: case 0x0c029ee0: return english + 0x40;
        case 0x0c1cf360: case 0x0c1d77e0: case 0x0c1d78a0:
        case 0x0c1d8380: case 0x0c1d8a60: case 0x0c1db280:
        case 0x0c1f65c0: case 0x0c1f6610: case 0x0c1f6770:
        case 0x0c1f6780: case 0x0c1f67e0: case 0x0c1f68a0:
        case 0x0c1f6950: case 0x0c1f69d0: case 0x0c1f6ac0:
        case 0x0c1f6f80: case 0x0c1fd060: case 0x0c212960:
        case 0x0c2223b8: case 0x0c228dc0: return english + 0x7140;
        default: throw std::runtime_error("Unknown car capture symbol: " + hex(english));
    }
}
}
