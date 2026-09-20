#include "original_rival_initialization.h"
#include <algorithm>
#include <stdexcept>

namespace idas3::original {
unsigned originalRivalCarIndex(unsigned profile){
    // Original32-u32 table31FE9C, canonical efda831f... image.
    static constexpr std::array<unsigned,32> cars{2,2,16,7,12,5,14,30,10,22,22,24,13,9,1,0,25,15,17,15,20,19,3,22,0,29,29,0,0,0,0,0};
    if(profile>=cars.size())throw std::out_of_range("Original rival profile outside32-car table");return cars[profile];
}
OriginalRivalInitializationResult initializeOriginalRival(OriginalRivalState& r,
    OriginalActorState& pub,std::array<OriginalCollisionQuery,4>& queries,
    const OriginalRivalInitializationInputs& in){
    if(in.condition0C9015CC>=18||in.enemyId0C9015E0>=32)throw std::invalid_argument("Original rival initialization selection");
    const auto profile=in.profileMode0C901648==2?31u:in.enemyId0C9015E0;
    auto level=in.level0C9015D0;
    if(in.condition0C9015CC<=2&&std::bit_cast<std::int32_t>(level)>5)level=5;
    const auto slot=unsigned(std::clamp(in.actorSlot,0,8)),car=originalRivalCarIndex(profile);
    r.setu(0,1);r.setu(8,std::uint32_t(std::clamp(in.field8,0,8)));
    for(unsigned offset:{4u,12u,16u,20u,44u,48u,52u,56u,60u,68u,72u,80u,84u,92u,96u,100u,108u,112u,116u,120u,124u,128u,132u,136u,176u,184u,188u,196u,236u,240u,244u,376u,380u,384u,388u,392u,396u,492u,496u,500u,504u,508u,512u,516u,520u,524u,528u,532u,536u})r.setu(offset,0);
    for(unsigned i=0;i<3;++i){r.setf(200+i*4,in.position[i]);pub.setf(i*4,in.position[i]);r.setf(224+i*4,in.angles[i]);}
    r.setf(172,in.angles[1]+std::bit_cast<float>(0x40490fdbu));
    for(unsigned i=0;i<4;++i)r.setu(684+i*4,(i+1)*4096);
    pub.setu(80,((pub.u(80)&0xffffe0ffu)|(slot<<8))&0xffffffc0u|car);
    for(unsigned offset:{64u,68u,72u,76u})pub.setu(offset,0);
    for(auto& query:queries)clearOriginalCollisionQuery(query);
    return {profile,profile,level,0,slot,car,profile==26};
}
}
