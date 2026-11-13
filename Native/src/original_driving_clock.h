#pragma once
#include <algorithm>

namespace idas3 {
// GDS-0033's captured SPG_LOAD=0x02110353, SPG_CONTROL=0 and
// FB_R_CTRL=0x00800005 specify 852 pixels x 530 scanlines at 27 MHz.
// The live original advances one driving frame per vblank. Use the nominal
// pixel-clock period, not a rounded 60 Hz or Flycast's integer-cycle truncation
// (59.793771 Hz). This is a wall-clock scheduler only: original numerical
// frame units, race timer increments and recorded frame indices stay intact.
class OriginalDrivingClock {
public:
    static constexpr double step=852.0*530.0/27000000.0;
    static constexpr double frequency=1.0/step;
    double accumulator=0;
    template<class F> int advance(double elapsed,F&& f){
        accumulator+=std::clamp(elapsed,0.0,0.1);
        int steps=0;
        while(accumulator+1e-12>=step&&steps<6){
            f();accumulator=std::max(0.0,accumulator-step);++steps;
        }
        return steps;
    }
    float alpha()const{return float(std::clamp(accumulator/step,0.0,1.0));}
    void reset(){accumulator=0;}
};
}
