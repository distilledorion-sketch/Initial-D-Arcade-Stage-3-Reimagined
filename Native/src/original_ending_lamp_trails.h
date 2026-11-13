#pragma once
#include "original_demo_lamp_trails.h"
#include "ending_replay.h"

namespace idas3 {
// The ending constructs replay owner1A8AE0 after its weather effect. Unlike
// the precomputed opening, its camera spring is live, so retain only the
// current lamp samples and their bounded nine-update projected history.
class OriginalEndingLampTrails {
public:
    void reset(){trails_.reset();active_=false;night_=updates_=0;camera_={};eye_={};}
    void begin(const std::filesystem::path& root,unsigned night,unsigned wet){
        reset();night_=night;active_=night==1||wet==1;
        if(!active_)return;
        if(!trails_.loaded())trails_.load(root,1);else trails_.reset(1);
    }
    bool active()const{return active_;}
    unsigned updates()const{return updates_;}
    const NativeTextureBank& textures()const{return trails_.textures;}
    const OriginalDemoLampTrails& samples()const{return trails_;}
    void advance(const EndingReplayFrame& frame,std::array<unsigned,2> models,
                 bool rivalVisible,const OriginalEndingCamera& camera,
                 const original::OriginalCollisionData& collision,bool sceneChanged=false){
        if(!active_)return;
        eye_=camera.eye();const auto target=camera.target();
        camera_=original::originalLookAtCameraWorld({eye_.x,eye_.y,eye_.z},{target.x,target.y,target.z}).elements;
        const auto view=original::originalSmokeView(camera.worldToView(),OriginalEndingCamera::fieldOfViewPhase());
        trails_.beginFrame(sceneChanged);
        for(unsigned slot=0;slot<2;++slot){const auto& car=frame.cars[slot];
            const bool available=car.visible&&(slot==0||rivalVisible);
            trails_.prepare(0,slot,available?models[slot]:0,car.lights&&available,
                car.visualWorld,view,eye_,collision,night_);
        }
        ++updates_;
    }
    unsigned append(Mesh& mesh,unsigned textureBase)const{
        if(!active_||!updates_)return 0;
        unsigned ribbons=0;
        for(unsigned slot=0;slot<2;++slot)ribbons+=trails_.append(mesh,0,slot,camera_,eye_,textureBase,true);
        return ribbons;
    }
private:
    OriginalDemoLampTrails trails_;
    bool active_=false;
    unsigned night_=0,updates_=0;
    std::array<float,16> camera_{};
    Vec3 eye_{};
};
}
