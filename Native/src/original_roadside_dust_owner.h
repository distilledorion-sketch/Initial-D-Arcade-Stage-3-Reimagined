#pragma once
#include "original_car_assembly.h"
#include "original_matrix.h"
#include <cmath>

namespace idas3::original {
struct OriginalRoadsideDustOwnerSelection {
    unsigned car=0,slotBase=0;
    float displayGain=1.f;
};
//06088C..060964 chooses one car and stops the opposite four wheel slots.
// Preserve the unsigned mode test and the separately published player flag.
inline OriginalRoadsideDustOwnerSelection originalRoadsideDustOwnerSelection(
        unsigned numericRaceMode,bool playerRequested){
    return playerRequested||numericRaceMode-2u<=1u
        ?OriginalRoadsideDustOwnerSelection{0,0,1.f}
        :OriginalRoadsideDustOwnerSelection{1,4,.2f};
}

// ACar::028940 reads the authored wheel translations, before wheel-offset
// customization, suspension or the host's collision-projected smoke points.
inline std::array<std::array<float,3>,4> originalCarEffectOrigins(const OriginalCarParts& parts){
    std::array<std::array<float,3>,4> out;
    for(unsigned i=0;i<4;++i){
        out[i]=parts.transforms[i<2?14:15].translation;
        if(i&1)out[i][0]=-out[i][0];
    }
    return out;
}

struct OriginalRoadsideDustOwner {
    std::array<std::array<float,3>,4> positions{}; // effect order, not contact order
    OriginalMatrix footprint;
    float displacementMagnitude=0;
    //0609F2..060A04, passed to each CarSmokeDisp. Depth uses X and Z.
    static constexpr std::array<float,3> dimensions{1.f,.8f,2.1f};
};

inline OriginalRoadsideDustOwner originalRoadsideDustOwner(
        const OriginalMatrix& carWorld,
        const std::array<std::array<float,3>,4>& origins,
        const std::array<float,3>& actorDisplacement){
    OriginalRoadsideDustOwner out;out.footprint=carWorld;
    //060A82..060A98: translate the enclosing identity UP first, then multiply
    // the copied ACar matrix. This is T(0,.1,0)*carWorld, not carWorld*T:
    // the lift stays vertical on banked/sloping roads. The original copied
    // footprint is unchanged;1F64A0 multiplies XF, it does not store a matrix.
    auto lifted=originalIdentityMatrix();translateOriginalMatrix(lifted,{0,.1f,0});
    OriginalMatrix attachment;
    for(unsigned column=0;column<4;++column){
        std::array<float,4> input;
        for(unsigned row=0;row<4;++row)input[row]=carWorld.elements[column*4+row];
        const auto value=transformOriginalVector(lifted,input);
        for(unsigned row=0;row<4;++row)attachment.elements[column*4+row]=value[row];
    }
    for(unsigned i=0;i<4;++i)out.positions[i]=transformOriginalPoint(attachment,origins[i]);
    //1F6C90: the source actor's per-tick displacement, not render dt or km/h.
    double squared=double(actorDisplacement[0])*double(actorDisplacement[0]);
    squared+=double(actorDisplacement[1])*double(actorDisplacement[1]);
    squared+=double(actorDisplacement[2])*double(actorDisplacement[2]);squared+=0.0;
    out.displacementMagnitude=std::sqrt(float(squared));
    return out;
}
}
