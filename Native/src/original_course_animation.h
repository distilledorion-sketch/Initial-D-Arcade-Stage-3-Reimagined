#pragma once
#include "native_assets.h"
#include "original_matrix.h"

namespace idas3 {
// Akagi primary draw 1A06D2..0720 / 1A1452..149E. The source stores
// this phase at course+0x56C and advances it once per primary scene call.
// The host calls advance once per course tick, never per camera/mirror pass.
class OriginalCourseAnimation {
public:
    void reset(unsigned courseIndex,bool night,bool wet);
    void advance();
    bool active()const{return active_;}
    std::uint16_t phase()const{return phase_;}
    std::uint32_t chunk()const{return nightConstructor_?84u:60u;}
    original::OriginalMatrix matrix(const original::OriginalFscaTable&)const;
    // Replaces the already selected original prop instance in the world
    // assembly, retaining its draw position and material bank.
    unsigned apply(NativeAssembly&,const original::OriginalFscaTable&)const;
private:
    std::uint16_t phase_=0;
    bool active_=false,nightConstructor_=false;
};
}
