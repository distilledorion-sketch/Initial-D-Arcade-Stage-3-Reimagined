#pragma once
#include "renderer.h"
#include <array>
#include <fstream>

namespace idas3 {
// The original hilight owner is separate from popup lamp meshes and the road
// beam. All state here is presentation-only: no new collision queries, random
// draws, or simulation timers. Each camera gets its own billboard and mask.
class HeadlightGlare {
public:
    NativeTextureBank textures;
    void load(const std::filesystem::path& root){
        if(!model_.chunks.empty())return;
        const auto path=root/"data/original_assets/effects/hilight";
        auto model=NativeModel::load(path/"hilight.idasmesh");
        auto bank=NativeTextureBank::load(path/"textures/textures.idastex");
        std::ifstream file(path/"anchors.bin",std::ios::binary);
        std::array<char,8> magic{};file.read(magic.data(),magic.size());
        const auto read=[&](auto& value){file.read(reinterpret_cast<char*>(&value),sizeof value);};
        unsigned count=0;read(count);
        if(magic!=std::array<char,8>{'I','D','3','H','L','G','1',0}||count!=35||model.chunks.size()!=4||bank.size()!=4)
            throw std::runtime_error("Invalid original headlight glare assets");
        for(auto& car:cars_){read(car.count);read(car.style);
            if(car.count<2||car.count>4||car.style>=model.chunks.size())throw std::runtime_error("Invalid lamp anchors");
            for(unsigned i=0;i<car.count;++i){read(car.positions[i]);const auto p=car.positions[i];
                if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))throw std::runtime_error("Invalid lamp position");}
        }
        if(!file||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Incomplete lamp anchors");
        model_=std::move(model);textures=std::move(bank);
    }
    static float strength(Vec3 lamp,Vec3 forward,Vec3 eye){
        const auto delta=eye-lamp;
        //0D3818:100m horizontal range;0D3A28: quantized sixth-power facing.
        if(delta.x*delta.x+delta.z*delta.z>10000.f||dot(delta,delta)<.0001f)return 0;
        const float facing=std::clamp(dot(normalized(delta),forward),0.f,1.f);
        const float square=facing*facing;
        return float(unsigned(square*square*square*255.f))/255.f;
    }
    unsigned append(Mesh& mesh,unsigned car,Vec3 body,float yaw,float pitch,float roll,
                    bool lightsOn,float opening,Vec3 eye,Vec3 target,Vec3 cameraUp,
                    unsigned textureBase,unsigned viewMask)const{
        if(!lightsOn||opening<=0||car>=cars_.size()||model_.chunks.empty())return 0;
        const auto rotate=[&](Vec3 p){
            const float cr=std::cos(roll),sr=std::sin(roll),cp=std::cos(pitch),sp=std::sin(pitch);
            const Vec3 r{cr*p.x-sr*p.y,sr*p.x+cr*p.y,p.z};
            return right(yaw)*r.x+Vec3{0,cp*r.y-sp*r.z,0}+forward(yaw)*(sp*r.y+cp*r.z);
        };
        const auto view=normalized(target-eye),across=normalized(cross(view,cameraUp)),up=normalized(cross(across,view));
        unsigned submitted=0;const auto& lamps=cars_[car];
        for(unsigned i=0;i<lamps.count;++i){
            //0D39E8..0D3A14 moves the optical quad0.3m along the car's
            // normalized forward column, clear of the lamp/body surface.
            const auto center=body+rotate(lamps.positions[i]+Vec3{0,0,.3f});
            if(dot(center-eye,view)<=0)continue;
            const float alpha=strength(center,rotate({0,0,1}),eye)*std::clamp(opening,0.f,1.f);
            if(alpha<=0)continue;
            for(const auto& batch:model_.chunks[lamps.style].batches){
                // Keep source blending/UVs. Depth testing hides lamps behind
                // scenery; no depth writes or face culling for optical quads.
                const auto isp=(batch.ich[1]&~(3u<<27))|(1u<<26);
                mesh.beginRange(textureBase+batch.material[9],batch.ich[2],batch.ich[0],isp,
                    batch.material[2],true,true,0,false,std::nullopt,false,viewMask);
                for(auto index:batch.indices){const auto& v=batch.vertices[index];
                    //0D40DC scales the source one-unit sprite to1.5m.
                    mesh.vertices.push_back({center+across*(v.position.x*1.5f)+up*(v.position.y*1.5f),
                        -view,{1,1,1,alpha},v.u,v.v});++mesh.ranges.back().count;
                }
            }++submitted;
        }return submitted;
    }
private:
    struct Car {unsigned count=0,style=0;std::array<Vec3,4> positions{};};
    std::array<Car,35> cars_{};
    NativeModel model_;
};
}
