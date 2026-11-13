#pragma once
#include "replay_roadside_dust.h"
#include "roadside_dust_presentation.h"
#include <cstring>
#include <map>

namespace idas3 {
inline ReplayRoadsideDustFrame captureReplayRoadsideDust(unsigned mode,bool playerRequested,
        const std::array<original::OriginalRoadsideDustCar,2>& cars){
    const auto selected=original::originalRoadsideDustOwnerSelection(mode,playerRequested);
    const auto& car=cars[selected.car];ReplayRoadsideDustFrame frame;
    frame.flags=selected.car|(car.available?2u:0u);
    if(car.available){
        frame.displacement=car.owner.displacementMagnitude;frame.footprint=car.owner.footprint.elements;frame.positions=car.owner.positions;
        for(unsigned i=0;i<4;++i)frame.surfaces|=std::uint32_t(car.wheelSurfaces[i])<<(i*8);
    }
    return frame;
}
inline void advanceReplayRoadsideDust(RoadsideDustPresentation& dust,const ReplayRoadsideDustFrame& frame){
    std::array<original::OriginalRoadsideDustCar,2> cars;
    const unsigned selected=frame.flags&1;auto& car=cars[selected];car.available=(frame.flags&2)!=0;
    car.owner.displacementMagnitude=frame.displacement;car.owner.footprint.elements=frame.footprint;car.owner.positions=frame.positions;
    for(unsigned i=0;i<4;++i)car.wheelSurfaces[i]=std::uint8_t(frame.surfaces>>(i*8));
    dust.advance(7,0,0,0,selected==0,cars);
}

// Reconstruct every accepted source tick, including the pre-race prefix.
// Checkpoints contain particles AND the private RNG. Repainting, slow motion,
// backward seeks and jumping directly to the finish must give the same state.
class ReplayRoadsideDustPlayback {
public:
    void clear(){frames_.clear();checkpoints_.clear();step_=0;}
    void load(std::span<const std::uint8_t> bytes,RoadsideDustPresentation& dust){
        if(bytes.size()%ReplayRoadsideDustFrame::bytes||bytes.size()/ReplayRoadsideDustFrame::bytes>ReplayRoadsideDustFrame::maxFrames)
            throw std::invalid_argument("Invalid replay dust timeline size");
        std::vector<ReplayRoadsideDustFrame> frames(bytes.size()/ReplayRoadsideDustFrame::bytes);
        if(!bytes.empty())std::memcpy(frames.data(),bytes.data(),bytes.size());
        for(const auto& frame:frames)if(!frame.valid())throw std::invalid_argument("Invalid replay dust input");
        clear();frames_=std::move(frames);dust.reset();checkpoints_.emplace(0,dust.checkpoint());
    }
    void seek(unsigned step,RoadsideDustPresentation& dust){
        if(step>frames_.size())throw std::out_of_range("Replay dust step outside timeline");
        if(step==step_)return;
        auto nearest=checkpoints_.upper_bound(step);
        if(nearest==checkpoints_.begin())throw std::logic_error("Replay dust timeline not loaded");
        --nearest;
        if(step<step_||nearest->first>step_){step_=nearest->first;dust.restore(nearest->second);}
        while(step_<step){
            advanceReplayRoadsideDust(dust,frames_[step_++]);
            if(step_%300==0)checkpoints_.try_emplace(step_,dust.checkpoint());
        }
    }
    unsigned step()const{return step_;}
private:
    std::vector<ReplayRoadsideDustFrame> frames_;
    std::map<unsigned,RoadsideDustPresentation::Checkpoint> checkpoints_;
    unsigned step_=0;
};
}
