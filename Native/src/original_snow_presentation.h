#pragma once
#include "original_snow.h"
#include "renderer.h"

namespace idas3 {
//1AD820 replay snow: immutable world-space geometry prepared at source cadence.
//Uses rain texture0; it has no connection to the host's live-race snow emitter.
class OriginalSnowPresentation {
public:
    bool active()const{return active_;}
    unsigned updates()const{return updates_;}
    const original::OriginalSnowFields& fields()const{return fields_;}
    void reset(){fields_={};active_=false;updates_=0;}
    void begin(){reset();active_=true;}
    void advance(const original::OriginalMatrix& view,unsigned& random){
        if(!active_)return;
        fields_.advance(view,1,[&]{random=random*0x41c64e6du+12345u;return float((random>>16)&32767u)/32768.f;});
        ++updates_;
    }
    unsigned append(Mesh& mesh,unsigned textureBase)const{
        if(!active_||!updates_)return 0;
        unsigned count=0;
        for(unsigned i=0;i<fields_.particles.size();++i)if(fields_.visible[i]){
            //Source texture0 is32x32 (U/V size2/2). Keep source depth, blend,
            //diffuse and fog behavior; rendering must not advance state or RNG.
            mesh.beginRange(textureBase,original::snowTspWithoutSize|18u,original::snowPcw,
                original::snowIsp,original::snowMaterialFlags,true,true,0,false,std::nullopt,false,1);
            const auto quad=fields_.quad(i);
            constexpr float gray=136.f/255.f;
            for(unsigned vertex:{0u,1u,2u,2u,1u,3u}){
                const auto& p=quad[vertex];mesh.vertices.push_back({{p[0],p[1],p[2]},
                    {0,1,0},{gray,gray,gray,1},vertex%2?1.f:0.f,vertex<2?1.f:0.f});
                ++mesh.ranges.back().count;
            }
            ++count;
        }
        return count;
    }
private:
    original::OriginalSnowFields fields_;
    bool active_=false;
    unsigned updates_=0;
};
}
