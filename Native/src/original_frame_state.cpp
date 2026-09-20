#include "original_frame_state.h"
#include <algorithm>
#include <cmath>

namespace idas3::original {
void publishOriginalActors(const OriginalActorState& player,const OriginalActorState& secondary,
    std::uint32_t mode,OriginalPublishedActors& published){
    std::copy_n(player.words.begin(),42,published.player0C8FF388.begin());
    if(mode==0)std::copy_n(secondary.words.begin(),42,published.secondary0C8FF430.begin());
}
bool applyOriginalRecovery(OriginalDriveState& drive,OriginalActorState& actor,
    OriginalPublishedActors& published,OriginalRecoveryState& recovery){
    const auto count=[](const OriginalDriveState& state){
        unsigned zero=0;for(std::size_t i=0;i<4;++i)if((state.u(0x164+i*4)&0xf0u)==0)++zero;return zero;
    };
    const auto current=count(drive);
    if(current==0){
        recovery.actor0C8FF580=published.player0C8FF388;
        std::copy_n(drive.words.begin(),272,recovery.drive0C9009F0.words.begin());
    }else if(current>2&&count(recovery.drive0C9009F0)==0){
        published.player0C8FF388=recovery.actor0C8FF580;
        std::copy_n(published.player0C8FF388.begin(),42,actor.words.begin());
        std::copy_n(recovery.drive0C9009F0.words.begin(),272,drive.words.begin());
        return true;
    }
    return false;
}
OriginalBodyCollisionEffects applyOriginalBodyCollisionResponse(OriginalDriveState& d,
    const OriginalBodyCollisionResult& collision,std::uint32_t& latch,std::uint32_t& seed){
    OriginalBodyCollisionEffects effects;
    if(collision.active!=0){
        const float prior=d.f(0x260)+d.f(0x264);
        if(std::bit_cast<float>(0x38D1B717u)>prior){
            if(latch==1){seed=seed*1103515245u+12345u;effects.cue=(((seed>>16)&0x7fffu)%2u)+1u;}
            else latch=1;
        }
        float x=0,z=0;
        if(std::isfinite(collision.x))x=collision.x;else ++effects.invalidScalarDiagnostics;
        if(std::isfinite(collision.z))z=collision.z;else ++effects.invalidScalarDiagnostics;
        d.setf(0x260,x*std::bit_cast<float>(0x3F0CCCCDu));
        d.setf(0x264,z*std::bit_cast<float>(0x3F0CCCCDu));
        d.setu(0x15C,1);
    }else{
        d.setf(0x260,d.f(0x260)*.5f);d.setf(0x264,d.f(0x264)*.5f);d.setu(0x15C,0);
    }
    for(auto offset:{0x260,0x264}){
        float value=d.f(offset);if(-2.f>value)value=-2.f;else if(value>2.f)value=2.f;d.setf(offset,value);
    }
    return effects;
}
}
