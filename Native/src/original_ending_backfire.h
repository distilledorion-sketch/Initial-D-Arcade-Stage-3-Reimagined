#pragma once
#include "backfire_presentation.h"
#include "ending_replay.h"

namespace idas3 {
class OriginalEndingBackfire {
public:
    bool active()const{return enabled_[0]||enabled_[1];}
    const auto& fields()const{return fields_;}
    const auto& enabled()const{return enabled_;}
    void reset(){fields_={};enabled_={};}
    void begin(const std::array<original::OriginalCarAppearanceConfig,2>& appearances,bool rival){
        reset();appearances_=appearances;
        for(unsigned i=0;i<2;++i)enabled_[i]=(!i||rival)&&BackfirePresentation::eligible(appearances[i]);
    }
    void advance(const EndingReplayFrame& frame,std::uint32_t& seed){
        if(!active())return;
        fields_.advance(enabled_,{frame.cars[0].actorFlags,frame.cars[1].actorFlags},seed);
    }
    unsigned append(Mesh& mesh,const BackfirePresentation& presentation,const EndingReplayFrame& frame,unsigned textureBase)const{
        unsigned count=0;
        for(unsigned i=0;i<2;++i)if(enabled_[i]&&frame.cars[i].visible){
            const auto& car=fields_.cars[i];
            count+=presentation.appendOriginal(mesh,car.presentedFrame,appearances_[i],
                frame.cars[i].visualWorld,car.variation,textureBase);
        }
        return count;
    }
    void appendLights(original::OriginalRaceLightingSets& sets,const EndingReplayFrame& frame)const{
        for(unsigned i=0;i<2;++i)if(enabled_[i]&&frame.cars[i].visible)
            BackfirePresentation::appendLight(sets,fields_.cars[i].presentedFrame,appearances_[i],frame.cars[i].visualWorld.elements);
    }
private:
    original::OriginalBackfireFields fields_;
    std::array<original::OriginalCarAppearanceConfig,2> appearances_;
    std::array<bool,2> enabled_{};
};
}
