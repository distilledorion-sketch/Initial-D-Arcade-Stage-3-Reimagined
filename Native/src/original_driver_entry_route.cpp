#include "original_driver_entry_route.h"
#include <bit>
namespace idas3::original {
void requestOriginalDriverSetup(OriginalBattleProfile& p){p.setu(1180,p.u(1180)|2u);}
void finishOriginalDriverSetupFlag(OriginalBattleProfile& p){
    if(p.u(1180)&2u)p.setu(1180,(p.u(1180)&~2u)|1u);
}
bool originalDriverSetupRequested(const OriginalBattleProfile& p){return (p.u(1180)&2u)!=0;}
bool originalDriverNameImportsExisting(const OriginalBattleProfile& p){return p.byte(1192)==2;}
std::uint32_t acceptOriginalDriverCard(OriginalBattleProfile& p,const OriginalCardAcceptanceRoutes& r){
    std::uint32_t target;
    if(p.byte(1192)==2){p.setu(1180,p.u(1180)|1u);target=r.converted84;}
    else if(std::bit_cast<std::int32_t>(p.u(1140))>0){
        target=(p.u(1180)&0x400000u)?r.integralColor88:r.ordinary76;
        p.setu(1180,p.u(1180)|1u);p.setu(1140,p.u(1140)-1u);
    }else{
        p.setu(1180,p.u(1180)|0x101u);p.setByte(1192,1);target=r.renewal80;
    }
    return (target<<16)|4u;
}
}

