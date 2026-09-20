#include "original_car_lighting.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
float f(std::uint32_t w){return std::bit_cast<float>(w);}
void append(OriginalCourseLighting&s,const OriginalCourseLight&l){
    // Actual0539E0 rejects the seventeenth registration.
    if(s.count<s.lights.size())s.lights[s.count++]=l;
}
OriginalCourseLighting borrowed(const OriginalCourseLighting&base,const OriginalCarLighting&car,const OriginalCarLightingSetup&setup){
    OriginalCourseLighting s;s.sourceRow=base.sourceRow;s.sourceAddress=0x0c035540;s.ambient=car.ambient;s.gain=car.gain;
    for(unsigned i=0;i<base.count;++i){const auto&l=base.lights[i];
        // Actual041BE0/03CEA0: Happo excludes both relative parallels; its
        // only daytime borrowed light is the wet owner's fixed light84.
        if(setup.course==4){
            if(setup.night){if(l.kind!=OriginalCourseLightKind::Spot)continue;}
            else if(!setup.wet||l.kind!=OriginalCourseLightKind::Parallel)continue;
        }
        // Actual derived1A43C0 excludes fixed slot2 on Shomaru wet routes.
        if(setup.course==6&&setup.wet&&l.kind==OriginalCourseLightKind::Parallel&&l.sourceSlot>=2)continue;
        append(s,l);
    }
    if(setup.course==4&&setup.night)append(s,originalHappoCarPointLight());
    return s;
}
}
OriginalCarLighting originalCarLighting(){
    OriginalCarLighting s;auto&l=s.ownSpot;l.kind=OriginalCourseLightKind::Spot;l.enabled=false;
    l.color={1,1,1};l.incomingDirection={0,-1,0};l.distance0=100;l.distance1=200;l.angle0=2731;l.angle1=4551;
    l.coefficientWords={0x4348bf80,0xc1863fc8};l.angleCosines={f(0x3f774661),f(0x3f680415)};
    return s;
}
OriginalLightVector originalCourseCarAmbient(const OriginalCarLightingSetup&setup){
    if(setup.course>=9)throw std::out_of_range("Original car ambient course");
    if(setup.course==4)return {.3f,.3f,.3f};
    return originalCourseLightingTableRow(setup.course,setup.night,setup.wet).ambient;
}
void publishOriginalCarLight(OriginalCarLighting&s,const OriginalLightMatrix&m){
    for(unsigned i=0;i<3;++i){
        // FTRV used by1F6AC0 at034B26..8C, before the body-normal offset.
        double v=double(m[i])*0.;v+=double(m[4+i])*(-.5);v+=double(m[8+i])*(-3.);v+=double(m[12+i]);
        s.ownSpot.position[i]=float(v);s.ownSpot.incomingDirection[i]=m[8+i];
    }
}
void setOriginalCarLightEnabled(OriginalCarLighting&s,bool enabled){s.ownSpot.enabled=enabled;}
void updateOriginalCarAmbient(OriginalCarLighting&s,const OriginalCarAmbientInputs&i){
    s.ambient=i.courseAmbient;
    if(i.course==6&&i.wet)for(auto&v:s.ambient)v=f(0x3f19999a)-v;
    if(i.rival&&i.night&&!i.rivalLightBeforeRequest){
        float factor=1.f;
        if(i.signedAdvantage>-5.f)factor=std::clamp(f(0xbe4ccccd)*i.signedAdvantage,0.f,1.f);
        for(auto&v:s.ambient)v*=factor;
    }
}
OriginalCourseLight originalHappoCarPointLight(){
    OriginalCourseLight l;l.kind=OriginalCourseLightKind::Point;l.sourceSlot=1068;l.enabled=true;
    l.color={f(0x3f4ccccd),f(0x3e99999a),0};l.position={f(0xc487b000),f(0x446ce000),f(0xc3798000)};
    // Converter054980 leaves raw direction at its reset value;2059A0 keeps
    // its transformed upper eight bits, unused by constant angular factor1.
    l.incomingDirection={0,-1,0};l.distance0=28;l.distance1=30;l.coefficientWords={0x43d2c160,0x00003f80};
    return l;
}
OriginalRaceLightingSets composeOriginalRaceLighting(const OriginalCourseLighting&base,
    const OriginalCarLighting&player,const OriginalCarLighting&rival,const OriginalCarLightingSetup&setup){
    if(setup.course>=9||setup.numericRaceMode>3||base.count>16||base.sourceRow/4!=setup.course)
        throw std::invalid_argument("Invalid original race light setup");
    OriginalRaceLightingSets out;out.course=base;out.hasRival=setup.numericRaceMode!=2&&setup.numericRaceMode!=3;
    out.player=borrowed(base,player,setup);
    if(out.hasRival)out.rival=borrowed(base,rival,setup);
    // Initial0633C0,063B60 append once; composing copies prevents repeated
    // render/repaint from accumulating duplicated source registrations.
    if(setup.night){append(out.course,player.ownSpot);
        if(out.hasRival){append(out.rival,player.ownSpot);append(out.course,rival.ownSpot);append(out.player,rival.ownSpot);}}
    return out;
}
} // namespace idas3::original
