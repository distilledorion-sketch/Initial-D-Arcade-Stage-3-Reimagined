#pragma once
#include "original_tire_marks.h"
#include "ending_replay.h"
#include "renderer.h"
namespace idas3 {
class OriginalEndingTireMarks {
public:
    void reset(){system_.reset();active_=false;updates_=0;}
    void begin(bool wet){reset();active_=!wet;}
    bool active()const{return active_;}unsigned updates()const{return updates_;}const auto& system()const{return system_;}
    void advance(const EndingReplayFrame& frame,const std::array<std::array<std::array<float,3>,4>,2>& origins){
        if(!active_)return;
        std::array<original::OriginalTireCar,2> cars;
        for(unsigned i=0;i<2;++i)cars[i]={frame.cars[i].visualWorld,origins[i],frame.cars[i].tireMarkIntensity};
        bool brown=false;for(auto surface:frame.cars[0].wheelSurfaces)brown|=(surface&8u)!=0;
        system_.advance(frame.cars[1].visible?2:1,cars,brown);++updates_;
    }
    unsigned append(Mesh& mesh,const OriginalEndingCamera& camera)const{
        if(!active_||!updates_)return 0;
        const auto view=original::originalSmokeView(camera.worldToView(),camera.fieldOfViewPhase());
        const auto eye=camera.eye();unsigned drawn=0;
        const auto vertex=[](const original::OriginalTireSample& sample,unsigned edge){
            const auto& p=sample.edges[edge];const auto& n=sample.normal;const auto color=sample.color;
            return Vertex{{p[0],p[1],p[2]},{n[0],n[1],n[2]},
                {float((color>>16)&255)/255.f,float((color>>8)&255)/255.f,float(color&255)/255.f,float(color>>24)/255.f}};
        };
        for(auto slot:system_.order())for(const auto& strip:system_.stroke(slot).strips()){
            if(strip.samples.size()<2||!original::originalTireStripVisible(strip,view,{eye.x,eye.y,eye.z}))continue;
            mesh.beginRange(0xffffffff,original::tireMarkTsp,original::tireMarkPcw,original::tireMarkIsp,
                original::tireMarkGmp,true,false,0,false,std::nullopt,false,1);
            // 1D0C40 walks each edge list from newest to oldest.
            for(std::size_t i=strip.samples.size()-1;i>0;--i){
                const auto a=vertex(strip.samples[i],0),b=vertex(strip.samples[i],1),
                    c=vertex(strip.samples[i-1],0),d=vertex(strip.samples[i-1],1);
                mesh.vertices.insert(mesh.vertices.end(),{a,b,c,b,d,c});mesh.ranges.back().count+=6;
            }
            ++drawn;
        }
        return drawn;
    }
private:
    original::OriginalTireMarkSystem system_;bool active_=false;unsigned updates_=0;
};
}
