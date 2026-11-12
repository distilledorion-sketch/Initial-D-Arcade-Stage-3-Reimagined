#pragma once
#include "replay_car_smoke.h"
#include "car_smoke_state.h"
#include <cstring>
#include <map>

namespace idas3 {
inline ReplayCarSmokeFrame captureReplayCarSmoke(const original::OriginalCarSmokeScene& scene,
        const std::array<original::OriginalCarSmokeInput,2>& cars,unsigned randomBefore,bool alternate=false){
    if(scene.course>=9||scene.night>1||scene.weather>1||scene.profileCourse>=9)
        throw std::invalid_argument("Invalid recorded smoke scene");
    const auto selected=original::originalRoadsideDustOwnerSelection(scene.numericRaceMode,scene.playerRequested);
    const auto& car=cars[selected.car];ReplayCarSmokeFrame frame;
    frame.owner.flags=selected.car|(car.available?2u:0u);
    if(car.available){
        frame.owner.displacement=car.owner.displacementMagnitude;frame.owner.footprint=car.owner.footprint.elements;frame.owner.positions=car.owner.positions;
        for(unsigned i=0;i<4;++i){frame.owner.surfaces|=unsigned(car.wheelSurfaces[i])<<(8*i);frame.intensity|=unsigned(car.intensity[i])<<(8*i);}
    }
    frame.scene=scene.course|(scene.night<<4)|(scene.weather<<5)|(scene.profileCourse<<6)|
        (unsigned(scene.playerRequested)<<10)|(unsigned(alternate)<<11);
    frame.randomBefore=randomBefore;return frame;
}
inline void advanceReplayCarSmoke(CarSmokeState& smoke,const ReplayCarSmokeFrame& frame){
    if(!frame.valid())throw std::invalid_argument("Invalid replay smoke input");
    const unsigned selected=frame.owner.flags&1;
    std::array<original::OriginalCarSmokeInput,2> cars;auto& car=cars[selected];
    car.available=(frame.owner.flags&2)!=0;car.owner.displacementMagnitude=frame.owner.displacement;
    car.owner.footprint.elements=frame.owner.footprint;car.owner.positions=frame.owner.positions;
    for(unsigned i=0;i<4;++i){car.wheelSurfaces[i]=std::uint8_t(frame.owner.surfaces>>(8*i));car.intensity[i]=std::uint8_t(frame.intensity>>(8*i));}
    const auto bits=frame.scene;
    const original::OriginalCarSmokeScene scene{bits&15,(bits>>4)&1,(bits>>5)&1,(bits>>6)&15,
        selected?0u:2u,(bits&0x400)!=0};
    smoke.advanceRecorded(scene,cars,frame.randomBefore);
}
class ReplayCarSmokePlayback {
public:
    void clear(){frames_.clear();checkpoints_.clear();step_=0;}
    bool empty()const{return frames_.empty();}
    void load(std::span<const std::uint8_t> bytes,CarSmokeState& smoke){
        if(bytes.size()%ReplayCarSmokeFrame::bytes||bytes.size()/ReplayCarSmokeFrame::bytes>ReplayCarSmokeFrame::maxFrames)
            throw std::invalid_argument("Invalid replay smoke timeline size");
        std::vector<ReplayCarSmokeFrame> frames(bytes.size()/ReplayCarSmokeFrame::bytes);
        if(!bytes.empty())std::memcpy(frames.data(),bytes.data(),bytes.size());
        for(const auto& frame:frames)if(!frame.valid()||
                ((frame.scene^frames.front().scene)&ReplayCarSmokeFrame::fixedSceneMask))
            throw std::invalid_argument("Invalid replay smoke timeline");
        // Validate the whole replacement before touching the active playback.
        CarSmokeState initial;const auto scene=frames.empty()?0u:frames.front().scene;
        initial.reset((scene>>5)&1,(scene&0x800)!=0);
        std::map<unsigned,CarSmokeState> checkpoints;checkpoints.emplace(0,initial);
        frames_=std::move(frames);checkpoints_=std::move(checkpoints);step_=0;smoke=std::move(initial);
    }
    void seek(unsigned step,CarSmokeState& smoke){
        if(step>frames_.size())throw std::out_of_range("Replay smoke step outside timeline");
        if(step==step_)return;
        auto nearest=checkpoints_.upper_bound(step);
        if(nearest==checkpoints_.begin())throw std::logic_error("Replay smoke timeline not loaded");
        --nearest;
        if(step<step_||nearest->first>step_){step_=nearest->first;smoke=nearest->second;}
        while(step_<step){advanceReplayCarSmoke(smoke,frames_[step_++]);
            if(step_%300==0)checkpoints_.try_emplace(step_,smoke);}
    }
    unsigned step()const{return step_;}
private:
    std::vector<ReplayCarSmokeFrame> frames_;
    std::map<unsigned,CarSmokeState> checkpoints_;
    unsigned step_=0;
};
}
