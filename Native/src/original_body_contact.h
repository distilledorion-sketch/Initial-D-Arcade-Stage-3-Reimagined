#pragma once
#include "original_frame_state.h"
#include "original_matrix.h"
#include "original_rival.h"

namespace idas3::original {
struct OriginalBodyShape {
    //624-byte source record: ID, half extents, pose, count,8 contacts,
    // bounding radius at492, world matrix496, rigid inverse560.
    std::array<std::uint32_t,156> words{};
    std::uint32_t u(std::size_t offset)const{return words.at(offset/4);}
    float f(std::size_t offset)const{return std::bit_cast<float>(u(offset));}
    void setu(std::size_t offset,std::uint32_t value){words.at(offset/4)=value;}
    void setf(std::size_t offset,float value){setu(offset,std::bit_cast<std::uint32_t>(value));}
};
struct OriginalBodyContactState {
    std::array<OriginalBodyShape,2> shapes0C401B04{};
    std::uint32_t count0CA9B360=0;
    std::array<std::array<float,3>,32> intersections0CA9B364{};
};
// Exact157880 entry through1578C4: published actor conversion157800,
// pair preparation0C06C0 and complete oriented-box contact solver0C04C0.
// Uses previously published poses. No rival-active filter is added: source
// publication/setup policy owns whether the secondary actor is meaningful.
// Shape geometry2700F4 is byte-identical to rival-pack2716A8 for all35 cars.
OriginalBodyCollisionResult produceOriginalBodyContact(const OriginalPublishedActors& published,
    OriginalBodyContactState& state,const OriginalRivalData& data,const OriginalFscaTable& fsca);
// Both results from one canonical slot0/slot1 solve, before either car moves.
std::array<OriginalBodyCollisionResult,2> produceOriginalBodyPairContact(const OriginalPublishedActors& published,
    OriginalBodyContactState& state,const OriginalRivalData& data,const OriginalFscaTable& fsca);
} // namespace idas3::original
