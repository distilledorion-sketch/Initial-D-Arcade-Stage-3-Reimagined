#pragma once
#include "original_matrix.h"
#include "original_smoke_view.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace idas3::original {
using OriginalTirePoint=std::array<float,3>;
using OriginalTireEdges=std::array<OriginalTirePoint,2>;
struct OriginalTireCar {
    OriginalMatrix world=originalIdentityMatrix();
    std::array<OriginalTirePoint,4> origins{};
    std::array<std::uint8_t,4> intensity{};
};
struct OriginalTireContact {OriginalTirePoint position{},normal{};};
// E4000: translate the render stack in world Y before multiplying the
// recorded visual matrix. The contact normal is the unmodified Y column.
inline OriginalTireContact originalTireContact(const OriginalTireCar& car,unsigned wheel){
    auto shift=originalIdentityMatrix();translateOriginalMatrix(shift,{0,.1f,0});
    OriginalMatrix world;
    for(unsigned column=0;column<4;++column){
        const unsigned i=column*4;
        const auto value=transformOriginalVector(shift,{car.world.elements[i],car.world.elements[i+1],
            car.world.elements[i+2],car.world.elements[i+3]});
        for(unsigned row=0;row<4;++row)world.elements[i+row]=value[row];
    }
    return {transformOriginalPoint(world,car.origins.at(wheel)),
        {car.world.elements[4],car.world.elements[5],car.world.elements[6]}};
}
// E4000 tracks only the two rear wheels of at most two replay actors. A zero
// intensity ends a stroke; a later nonzero value must acquire a new stroke.
class OriginalReplayTireAdmission {
public:
    void reset(){active_={};handles_={};}
    const auto& active()const{return active_;}
    const auto& handles()const{return handles_;}
    template<class Acquire,class Append> void advance(int count,const std::array<OriginalTireCar,2>& cars,
            Acquire&& acquire,Append&& append){
        for(int car=0;car<std::min(count,2);++car)for(unsigned rear=0;rear<2;++rear){
            const auto value=cars[car].intensity[rear+2];
            if(!active_[car][rear]&&value){handles_[car][rear]=acquire();active_[car][rear]=true;}
            if(active_[car][rear]&&!value)active_[car][rear]=false;
            if(active_[car][rear])append(handles_[car][rear],originalTireContact(cars[car],rear+2),value);
        }
    }
private:
    std::array<std::array<bool,2>,2> active_{};
    std::array<std::array<unsigned,2>,2> handles_{};
};
// 1502A0/1FD0A0 form a horizontal 0.2-unit-wide ribbon at the current point.
// A coincident/vertical step returns false and leaves the caller's output
// untouched, as the original does. Keep FMAC and FIPR rounding distinct.
inline bool originalTireEdges(const OriginalTirePoint& previous,const OriginalTirePoint& current,
        float halfWidth,OriginalTireEdges& out){
    auto above=current;above[1]+=1.f;
    OriginalTirePoint a,b;
    for(unsigned i=0;i<3;++i){a[i]=current[i]-previous[i];b[i]=above[i]-current[i];}
    OriginalTirePoint side{
        std::fma(a[1],b[2],-(a[2]*b[1])),std::fma(a[2],b[0],-(a[0]*b[2])),
        std::fma(a[0],b[1],-(a[1]*b[0]))};
    double square=double(side[0])*double(side[0]);
    square+=double(side[1])*double(side[1]);square+=double(side[2])*double(side[2]);square+=0.0;
    const float squared=float(square);if(squared==0)return false;
    const float inverse=1.f/std::sqrt(squared);
    for(unsigned i=0;i<3;++i){side[i]*=inverse;side[i]*=halfWidth;
        out[0][i]=current[i]+side[i];out[1][i]=current[i]-side[i];}
    return true;
}
struct OriginalTireSample {
    OriginalTireEdges edges{};
    OriginalTirePoint normal{};
    unsigned color=0;
};
struct OriginalTireStrip {
    std::vector<OriginalTireSample> samples;
    unsigned cadence=0;
    float maxX=-std::numeric_limits<float>::max(),maxZ=maxX,minX=-maxX,minZ=minX;
    OriginalTirePoint center{};float radiusSquared=0;
    void updateBounds(const OriginalTirePoint& p){
        maxX=std::max(p[0],maxX);maxZ=std::max(p[2],maxZ);minX=std::min(p[0],minX);minZ=std::min(p[2],minZ);
        center={(maxX+minX)*.5f,p[1],(maxZ+minZ)*.5f};
        const float x=maxX-center[0],z=maxZ-center[2];radiusSquared=std::fma(x,x,z*z);
    }
    void append(const OriginalTireSample& sample){samples.push_back(sample);updateBounds(sample.edges[0]);}
    // 150620 inserts a pair every fifth update; intervening frames replace
    // the newest pair. There are ten pairs after the two pool sentinels.
    bool advance(OriginalTireSample sample,bool brown){
        if(++cadence>=5){cadence=0;append(sample);return samples.size()==10;}
        if(brown)sample.color|=0x003c291b;
        samples.back()=sample;updateBounds(sample.edges[0]);return false;
    }
};
class OriginalTireStroke {
public:
    void reset(){strips_.clear();started_=hasEdges_=hasStrip_=false;previous_={};edges_={};}
    const auto& strips()const{return strips_;}
    const auto& previous()const{return previous_;}
    const auto& edges()const{return edges_;}
    bool started()const{return started_;}bool hasEdges()const{return hasEdges_;}bool hasStrip()const{return hasStrip_;}
    // 150B80 retains twenty strips per stroke, newest first. Acquiring a new
    // stroke resets its history; ending a mark does not fade previous strips.
    void advance(const OriginalTireContact& contact,unsigned intensity,bool brown){
        if(!started_){started_=true;previous_=contact.position;return;}
        OriginalTireEdges next=edges_;
        // The original leaves undefined stack coordinates on a zero-length
        // step. Retain the last valid pair instead of admitting invalid data.
        if(!originalTireEdges(previous_,contact.position,.1f,next)){previous_=contact.position;return;}
        const auto color=(intensity&255u)<<24;
        if(!hasStrip_){
            if(strips_.size()==20)strips_.pop_back();
            strips_.insert(strips_.begin(),OriginalTireStrip{});
            if(!hasEdges_){edges_=next;hasEdges_=true;}
            strips_.front().append({edges_,contact.normal,color});
            strips_.front().append({next,contact.normal,color});hasStrip_=true;
        }else{
            if(strips_.front().advance({next,contact.normal,color},brown))hasStrip_=false;
            edges_=next;
        }
        previous_=contact.position;
    }
private:
    std::vector<OriginalTireStrip> strips_;
    OriginalTirePoint previous_{};OriginalTireEdges edges_{};
    bool started_=false,hasEdges_=false,hasStrip_=false;
};
inline bool originalTireStripVisible(const OriginalTireStrip& strip,const OriginalSmokeView& view,const OriginalTirePoint& eye){
    const float depth=originalSmokeFrustumDepth(view,strip.center,std::sqrt(strip.radiusSquared));
    if(depth>0&&depth<80.f)return true;
    const float x=eye[0]-strip.center[0],z=eye[2]-strip.center[2];
    return strip.radiusSquared>std::fma(x,x,z*z);
}
class OriginalTireMarkSystem {
public:
    static constexpr unsigned capacity=50; // E2E30, not the live-race budget
    void reset(){admission_.reset();for(auto& stroke:strokes_)stroke.reset();order_.clear();acquired_=0;}
    unsigned acquire(){
        unsigned slot=unsigned(order_.size());
        if(slot==capacity){slot=order_.back();order_.pop_back();}
        strokes_[slot].reset();order_.insert(order_.begin(),slot);++acquired_;return slot+1;
    }
    void advance(int count,const std::array<OriginalTireCar,2>& cars,bool brown){
        admission_.advance(count,cars,[&]{return acquire();},[&](unsigned handle,const OriginalTireContact& point,unsigned strength){
            strokes_[handle-1].advance(point,strength,brown);
        });
    }
    const auto& order()const{return order_;}const auto& stroke(unsigned slot)const{return strokes_.at(slot);}
    const auto& admission()const{return admission_;}unsigned acquired()const{return acquired_;}
private:
    OriginalReplayTireAdmission admission_;
    std::array<OriginalTireStroke,capacity> strokes_;
    std::vector<unsigned> order_;
    unsigned acquired_=0;
};
inline constexpr unsigned tireMarkPcw=0x8a00072f,tireMarkIsp=0x9bc00000,tireMarkTsp=0x941824c0,tireMarkGmp=0xaa;
} // namespace idas3::original
