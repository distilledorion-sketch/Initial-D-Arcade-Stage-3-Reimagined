#pragma once
#include "original_replay_leaf_system.h"
#include "ending_replay.h"
#include "renderer.h"

namespace idas3 {
// The ending's AReplay leaf owner. The recorded contact bytes and visual
// transform drive the source numerical system; rendering only observes it.
class OriginalEndingLeaves {
public:
    void reset(){system_.reset();active_=false;updates_=0;}
    void begin(const std::filesystem::path& root){
        reset();
        if(model_.chunks.empty()){
            const auto base=root/"data/original_assets/environment/leaf";
            auto model=NativeModel::load(base/"leaf.idasmesh");
            auto textures=NativeTextureBank::load(base/"textures.idastex");
            if(model.chunks.size()!=8||textures.size()!=4)throw std::runtime_error("Original ending leaf assets missing");
            model_=std::move(model);textures_=std::move(textures);
        }
        active_=true;
    }
    bool active()const{return active_;}
    unsigned updates()const{return updates_;}
    const auto& system()const{return system_;}
    const auto& textures()const{return textures_;}
    void advance(const EndingReplayFrame& frame,const std::array<std::array<float,3>,4>& origins,
            const original::OriginalFscaTable& trig,unsigned& random){
        if(!active_)return;
        const auto& car=frame.cars[0];
        const auto input=original::originalReplayLeafFrame(car.visualWorld,origins,car.wheelSurfaces,car.displacement,trig);
        system_.advance(input,trig,[&]{random=random*0x41c64e6du+12345u;return float((random>>16)&32767u)/32768.f;});
        ++updates_;
    }
    unsigned append(Mesh& mesh,unsigned textureBase,const original::OriginalFscaTable& trig)const{
        if(!active_)return 0;
        NativeAssembly assembly;assembly.instances.reserve(system_.order().size());
        for(auto index:system_.order()){
            const auto& p=system_.particle(index);auto& instance=assembly.instances.emplace_back();instance.chunk=p.model;
            const auto matrix=original::originalActorMatrix(p.position,p.angles,trig);
            for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)
                instance.transform[row*4+col]=matrix.elements[col*4+row];
        }
        const auto first=mesh.ranges.size();mesh.originalCar(model_,assembly,{},0,textureBase);
        for(auto i=first;i<mesh.ranges.size();++i)mesh.ranges[i].viewMask=1;
        return unsigned(assembly.instances.size());
    }
private:
    original::OriginalReplayLeafSystem system_;
    NativeModel model_;
    NativeTextureBank textures_;
    bool active_=false;
    unsigned updates_=0;
};
}
