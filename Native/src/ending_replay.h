#pragma once
#include "car_wheel_pose.h"
#include "original_headlights.h"
#include "original_matrix.h"
#include "math_types.h"
#include <stdexcept>
#include <algorithm>
#include <array>
#include <vector>

namespace idas3 {
struct EndingCarFrame {
    Vec3 actor{},body{};
    float yaw=0,pitch=0,roll=0;
    CarWheelPose wheels;
    OriginalHeadlightState headlights;
    bool visible=false,lights=false,braking=false;
};
struct EndingReplayFrame {
    std::array<EndingCarFrame,2> cars;
    int pathIndex=0;
    Vec3 eye{},target{};
};
// A private, bounded presentation recording. It never depends on the optional
// replay archive and is never submitted as a time or written to the save card.
// The source ending reads the two-car 3600-frame ring at 63D438 (0EBF80/FC0).
class EndingReplay {
public:
    static constexpr std::size_t capacity=3600;
    void clear(){frames_.clear();next_=0;sealed_=false;}
    void record(EndingReplayFrame frame,const original::OriginalFscaTable& trig){
        if(sealed_)return;
        const auto& car=frame.cars[0];
        const auto matrix=original::originalActorMatrix({car.actor.x,car.actor.y,car.actor.z},
            {-car.pitch,wrapAngle(car.yaw-pi),-car.roll},trig);
        // Kind13's car-relative eye (0A33E0) and target from the original
        // o_ending_camera_00 record. Keep framing independent of gameplay's
        // selected chase/bumper camera. Source camera shake is not recreated.
        const auto eye=original::transformOriginalPoint(matrix,{1.72999906539917f,5.020020484924316f,-9.260116577148438f});
        const auto target=original::transformOriginalPoint(matrix,{.21999984979629517f,2.8399977684020996f,.030000044032931328f});
        frame.eye={eye[0],eye[1],eye[2]};frame.target={target[0],target[1],target[2]};
        if(frames_.size()<capacity)frames_.push_back(std::move(frame));
        else frames_[next_]=std::move(frame);
        next_=(next_+1)%capacity;
    }
    void seal(){sealed_=true;}
    bool ready()const{return sealed_&&frames_.size()>1;}
    std::size_t size()const{return frames_.size();}
    const EndingReplayFrame& frame(unsigned tick)const{
        if(frames_.empty())throw std::logic_error("Ending recording is empty");
        const auto offset=std::min<std::size_t>(tick,frames_.size()-1);
        return frames_.at(((frames_.size()==capacity?next_:0)+offset)%frames_.size());
    }
private:
    std::vector<EndingReplayFrame> frames_;
    std::size_t next_=0;
    bool sealed_=false;
};
}
