#pragma once
#include "original_feedback.h"
#include "original_vehicle.h"
#include <algorithm>
#include <array>

namespace idas3ffb {
// Keep original units and signs here. Device-axis conversion and user gain
// belong after the producer, not in its inputs or lookup-table indices.
inline OriginalInput feedbackInput(const idas3::original::OriginalVehicleState& vehicle,
        unsigned car,unsigned condition,unsigned savedSteering) {
    OriginalInput in;
    in.carIndex=car;in.condition=condition;in.cardSteeringLevel=savedSteering;
    const auto& d=vehicle.drive;
    in.steering1CC=d.f(0x1CC);in.headingFactor27C=d.f(0x27C);in.acceleration24C=d.f(0x24C);
    in.frontContact3B4=d.f(0x3B4);in.frontContact3B8=d.f(0x3B8);
    in.normalizedFrontContact3C4=d.f(0x3C4);in.normalizedFrontContact3C8=d.f(0x3C8);
    in.wallContact150=d.u(0x150);in.wallCorner154=d.u(0x154);in.wallMagnitude158=d.f(0x158);
    in.wet434=d.u(0x434);in.snow438=d.u(0x438);in.oil140=d.u(0x140);in.automaticBrake1A8=d.u(0x1A8);
    in.drive258=d.f(0x258);in.drive25C=d.f(0x25C);in.drive260=d.f(0x260);in.drive264=d.f(0x264);
    in.normalizedSpeed248=d.f(0x248);in.surface164=d.u(0x164);in.surface168=d.u(0x168);in.roadFlag14C=d.u(0x14C);
    const auto& t=vehicle.transmission;
    in.filteredRpm=t.filtered18;in.gear=t.gear00;in.gearSnapshot=t.snapshot04;
    in.transmissionNormalized=t.normalized20;
    return in;
}

// A presentation-only observer: one invocation per accepted local driving
// step, no physics writes and no hardware calls. Retain a transient's peak
// until the shared wheel/pad reader consumes it, so a one-tick event cannot
// disappear when several original ticks run before the next rendered frame.
// This is bounded host event delivery, not an extra arcade decay envelope.
class OriginalFeedbackPresentation {
public:
    void stop() noexcept {
        producer_.reset();latest_={};events_={};active_=false;
    }
    void advance(const OriginalInput& input,std::uint64_t tick) noexcept {
        if(active_&&tick==tick_)return;
        if(!active_||tick<tick_){producer_.reset();events_={};}
        tick_=tick;
        latest_=producer_.update(input);
        active_=latest_.valid;
        if(!active_){stop();return;}
        // A disabled or absent consumer must not replay an old impact later.
        // Each sample expires independently: an older, larger impact must not
        // hide a fresh smaller one when multiple online ticks precede a read.
        for(auto& event:events_)
            if(tick<event.tick||tick-event.tick>6)event={};
        events_[tick%events_.size()]={tick,latest_.rumbleAmplitude};
    }
    bool active() const noexcept {return active_;}
    std::uint64_t tick() const noexcept {return tick_;}
    const OriginalOutput& latest() const noexcept {return latest_;}
    int consumePeak() noexcept {
        int peak=0;
        for(const auto& event:events_)peak=std::max(peak,event.amplitude);
        events_={};return peak;
    }
    const OriginalProducerState& producerState() const noexcept {return producer_.state();}
private:
    OriginalProducer producer_;
    OriginalOutput latest_;
    std::uint64_t tick_=0;
    struct Event {std::uint64_t tick=0;int amplitude=0;};
    std::array<Event,7> events_{};
    bool active_=false;
};
}
