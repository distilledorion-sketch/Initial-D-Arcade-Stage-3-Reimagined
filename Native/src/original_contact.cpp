#include "original_contact.h"
#include <cmath>
#include <limits>

namespace idas3::original {
namespace {
float constant(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::uint32_t truncateOriginal(float value){
    if(value>=2147483648.0f)return 0x7FFFFFFFu;
    if(value<=-2147483648.0f||std::isnan(value))return 0x80000000u;
    return std::uint32_t(static_cast<std::int32_t>(value));
}
}
void prepareOriginalContactFrame(OriginalDriveState& d,OriginalActorState& a,
        OriginalWheelHistory& history){
    for(std::size_t axis=0;axis<3;++axis){
        const auto offset=axis*4;
        a.setf(0xC+offset,d.f(offset)-a.f(offset));
        a.setu(0x24+offset,a.u(offset));a.setu(0x30+offset,a.u(0x18+offset));
        a.setu(offset,d.u(offset));a.setu(0x18+offset,d.u(0xC+offset));
    }
    a.setf(0x3C,(d.f(0x1CC)*constant(0x40490FDB))*constant(0x3E70A3D7));
    for(std::size_t i=0;i<8;++i)a.setByte(0x54+i,std::uint8_t(d.u(0x414+i*4)));
    for(std::size_t i=0;i<4;++i)a.setByte(0x74+i,std::uint8_t(d.u(0x164+i*4)));
    for(std::size_t i=0;i<4;++i)a.setByte(0x9C+i,std::uint8_t(d.u(0x174+i*4)));
    a.setu(0x50,(a.u(0x50)&~0x4000u)|((d.u(0x1AC)&1u)<<14));
    a.setu(0x5C,(a.u(0x5C)&~3u)|(d.u(0x1B0)&1u)|((d.u(0x1B4)&1u)<<1));
    d.setu(0x1A8,(a.u(0x50)>>13)&1u);

    // Inputs are the ORIGINAL preceding contact outputs, not a constructed
    // tire law. These224/228 values are consumed on the nextCEC0 invocation.
    const float speed=d.f(0x238);
    const float complementFront=1.0f-d.f(0x3DC),complementRear=1.0f-d.f(0x3E0);
    const float frontDrag=(complementFront*d.f(0x1C4))*speed;
    const float rearDrag=(complementRear*d.f(0x1C4))*speed;
    const float pedalTerm=d.f(0x1B8)*complementRear;
    const float speedScale=constant(0x42480000)*speed;
    const float limitedSpeed=constant(0x42F00000)>speedScale?speedScale:constant(0x42F00000);
    const float rearDrive=pedalTerm*limitedSpeed;
    d.setf(0x228,rearDrag);d.setf(0x224,rearDrive);
    const auto frontIncrement=truncateOriginal((speed-frontDrag)*constant(0x4320F66E));
    const auto rearIncrement=truncateOriginal(((speed-rearDrag)+rearDrive)*constant(0x4320F66E));
    for(std::size_t wheel=0;wheel<4;++wheel){
        auto& counter=history.rotationCounters0CAA94B4[wheel];
        counter=(counter+(wheel<2?frontIncrement:rearIncrement))&0xFFFFu;
        a.setf(0x60+wheel*4,(float(counter)*constant(0x40490FDB))*constant(0x38000000));
    }
}
} // namespace idas3::original
