#pragma once
#include "headlight_glare.h"
#include "ending_replay.h"

namespace idas3 {
// E3700 gates the ending's optical headlight pass on night, separately from
// the road projection. Reuse the live-race lens adapter, including its close-
// camera attachment correction; do not create another glow or simulation.
class OriginalEndingHeadlightGlare {
public:
    void reset(){active_=false;}
    void begin(const std::filesystem::path& root,bool night){
        reset();if(night){glare_.load(root);active_=true;}
    }
    bool active()const{return active_;}
    const auto& textures()const{return glare_.textures;}
    unsigned append(Mesh& mesh,const EndingReplayFrame& frame,
            const std::array<unsigned,2>& cars,bool rivalVisible,
            const OriginalEndingCamera& camera,unsigned textureBase)const{
        if(!active_)return 0;
        unsigned submitted=0;
        for(unsigned slot=0;slot<2;++slot){
            const auto& car=frame.cars[slot];
            if(!car.visible||(slot&&!rivalVisible))continue;
            submitted+=glare_.append(mesh,cars[slot],car.body,car.yaw,car.pitch,car.roll,
                car.lights,car.headlights.fraction,camera.eye(),camera.target(),{0,1,0},textureBase,1);
        }
        return submitted;
    }
private:
    HeadlightGlare glare_;
    bool active_=false;
};
}
