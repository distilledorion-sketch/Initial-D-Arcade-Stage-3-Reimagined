#pragma once
#include "original_headlight_projection.h"
#include <stdexcept>

namespace idas3 {
// One ACar's independently retained projected geometry and road-query state.
// Rendering only reads model(); all source state transitions belong to 60 Hz.
class OriginalHeadlightPresentation {
public:
    void load(const std::filesystem::path& root){
        projection_.load(root);loaded_=true;reset();
    }
    void reset(){
        if(loaded_)projection_.reset();
        enabled_=false;trace_={};surface_={};
    }
    bool loaded()const{return loaded_;}
    bool enabled()const{return enabled_;}
    void request(bool on,const original::OriginalCollisionData& collision,
                 const original::OriginalCollisionQuery& bodyQuery,
                 const std::array<float,3>& actorPosition){
        if(on==enabled_)return;
        if(on){
            if(!loaded_)throw std::logic_error("Projected headlight assets are not loaded");
            //035160 refreshes ACar+AA0 at the attached actor before0D5320
            // copies all64 bytes. Keep presentation queries out of the solver.
            auto binding=bodyQuery;
            for(unsigned i=0;i<3;++i){binding.setf(32+4*i,actorPosition[i]);binding.setf(44+4*i,actorPosition[i]);}
            original::queryOriginalCollisionSurface(collision,binding,trace_,surface_);
            projection_.bindRoad(binding);
        }
        enabled_=on;
    }
    void publish(){if(enabled_)projection_.publish();}
    void advance(const original::OriginalCollisionData& collision,
                 const original::OriginalHeadlightProjection::Matrix& matrix){
        if(enabled_)projection_.advance(matrix,[&](auto& query){
            return original::queryOriginalCollisionSurface(collision,query,trace_,surface_);
        });
    }
    const NativeModel& model()const{return projection_.model();}
    const original::OriginalHeadlightProjection& projection()const{return projection_;}
private:
    original::OriginalHeadlightProjection projection_;
    original::OriginalTriangleSearchTrace trace_{};
    original::OriginalSurfaceScratch surface_{};
    bool loaded_=false,enabled_=false;
};
}
