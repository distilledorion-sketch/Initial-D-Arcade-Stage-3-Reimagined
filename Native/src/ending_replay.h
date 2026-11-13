#pragma once
#include "car_wheel_pose.h"
#include "original_headlights.h"
#include "original_matrix.h"
#include "original_ending_camera.h"
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
    // Exact published ACar world transform, before its body-only road offset.
    // Lighting must not reconstruct this from rounded host Euler angles.
    original::OriginalMatrix visualWorld=original::originalIdentityMatrix();
    // Replay smoke consumes actor+12/+84/+116 at this same capture boundary.
    // Speedometer readings or poses alone cannot reproduce its admission.
    std::array<float,3> displacement{};
    std::array<std::uint8_t,4> smokeIntensity{},wheelSurfaces{};
    // E4000 reads actor+88, independently of the actor+84 smoke intensity.
    std::array<std::uint8_t,4> tireMarkIntensity{};
    // Original actor+5C: preserve the backfire trigger independently of
    // engine audio, save-card/replay archive options, and the brake bit.
    std::uint32_t actorFlags=0;
};
struct EndingReplayFrame {
    std::array<EndingCarFrame,2> cars;
    int pathIndex=0;
    Vec3 cameraActorAngles{};
    Vec3 eye{},target{};
};
// A private, bounded presentation recording. It never depends on the optional
// replay archive and is never submitted as a time or written to the save card.
// The derived ending vtable387EBC overrides the ordinary replay getters:
// EC000..EC080 select a 5000-frame highlight at764A0C. EAC72 separately
// prepares the 3600-frame recent-race ring at63D438 for the second pass.
class EndingReplay {
public:
    static constexpr std::size_t capacity=3600;
    static constexpr std::size_t highlightCapacity=5000;
    static constexpr float highlightThreshold=std::bit_cast<float>(0x3edc28f6u);
    void clear(){frames_.clear();highlight_.clear();next_=0;sealed_=false;highlightStarted_=false;}
    void record(EndingReplayFrame frame,const original::OriginalFscaTable& trig,
                float courseFraction,int lap){
        if(sealed_)return;
        const auto& car=frame.cars[0];
        //03485C binds the raw actor angles, then applies the model half-turn
        // AFTER all three rotations. Reversing the host yaw loses that turn
        // (and float precision) and puts the credits camera ahead of the car.
        auto matrix=original::originalActorMatrix(
            {car.actor.x,car.actor.y-std::bit_cast<float>(0x3ca3d70au),car.actor.z},
            {frame.cameraActorAngles.x,frame.cameraActorAngles.y,frame.cameraActorAngles.z},trig);
        original::rotateOriginalMatrixPhase(matrix,1,0x8000u,trig);
        // Preserve the two raw kind13 anchors. The ending camera's spring
        // runs during playback, so discarded footage cannot pre-age it.
        constexpr auto e=OriginalEndingCamera::eyeOffset,t=OriginalEndingCamera::targetOffset;
        const auto eye=original::transformOriginalPoint(matrix,{e.x,e.y,e.z});
        const auto target=original::transformOriginalPoint(matrix,{t.x,t.y,t.z});
        frame.eye={eye[0],eye[1],eye[2]};frame.target={target[0],target[1],target[2]};
        // E11C0/E1200: strictly past43% on lap0, then record continuously
        // until5000 frames. Reversing or crossing a lap cannot restart it.
        if(!highlightStarted_&&lap==0&&courseFraction>highlightThreshold)highlightStarted_=true;
        if(highlightStarted_&&highlight_.size()<highlightCapacity)highlight_.push_back(frame);
        if(frames_.size()<capacity)frames_.push_back(std::move(frame));
        else frames_[next_]=std::move(frame);
        next_=(next_+1)%capacity;
    }
    void seal(){sealed_=true;}
    bool ready()const{return sealed_&&!highlight_.empty();}
    std::size_t size()const{return frames_.size();}
    std::size_t highlightSize()const{return highlight_.size();}
    const EndingReplayFrame& recentFrame(unsigned tick)const{
        if(frames_.empty())throw std::logic_error("Ending recording is empty");
        const auto offset=std::min<std::size_t>(tick,frames_.size()-1);
        return frames_.at(((frames_.size()==capacity?next_:0)+offset)%frames_.size());
    }
    const EndingReplayFrame& playbackFrame(unsigned tick)const{
        if(!ready())throw std::logic_error("Ending highlight is not sealed");
        if(tick<highlight_.size())return highlight_[tick];
        const auto end=(next_+capacity-1)%capacity;
        // EAC88 adds143 to the final ring index. It does NOT normalize a
        // positive overflow here;157640 wraps that cursor on its next step.
        const auto start=end+143;
        const auto elapsed=std::size_t(tick)-highlight_.size();
        const auto duration=start<capacity?capacity-142:end+2;
        const auto index=elapsed>=duration?(frames_.size()==capacity?next_:0):
            elapsed==0?start:start<capacity?(start+elapsed)%capacity:elapsed-1;
        // The source may read outside its ring for the first secondary frame,
        // or unwritten slots in a deliberately short capture. Keep a valid
        // pose for those reads without changing the source cursor timing.
        if(index>=frames_.size())return highlight_.back();
        return frames_[index];
    }
private:
    std::vector<EndingReplayFrame> frames_,highlight_;
    std::size_t next_=0;
    bool sealed_=false,highlightStarted_=false;
};
}
