#pragma once
#include "math_types.h"
#include "original_matrix.h"
#include "original_rear_view.h"

namespace idas3 {
// The recovered camera implementations remain Bumper and Chase. Natural is
// an optional host presentation and never enters their source camera logic.
enum class OriginalDrivingView {Bumper,Chase,Natural};
struct OriginalChaseFrame {
    Vec3 eye{},target{},up{0,1,0};
    float verticalFieldOfView=0;
    // Original camera-to-world matrix, before renderer view inversion.
    original::OriginalMatrix cameraWorld;
};
class OriginalChaseCamera {
public:
    static OriginalChaseCamera load(const std::filesystem::path& root,OriginalDrivingView view=OriginalDrivingView::Chase);
    void reset(){ready_=false;}
    // Call at 60 Hz using original actor XYZ and actor pitch/yaw/roll radians.
    // Inputs precede the car's ride-height lift and visual half-turn.
    const OriginalChaseFrame& update(Vec3 actorPosition,Vec3 actorAngles);
    const OriginalChaseFrame& frame()const{return frame_;}
    bool ready()const{return ready_;}
    Vec3 smoothedAngles()const{return angles_;}
    OriginalRearViewFrame rearView(Vec3 carBodyPosition,Vec3 actorAngles)const{
        return originalRearViewFrame(original::originalActorMatrix(
            {carBodyPosition.x,carBodyPosition.y,carBodyPosition.z},
            {actorAngles.x,actorAngles.y,actorAngles.z},trig_));
    }
    static constexpr float sourceVerticalFieldOfView=1.2333658933639526f;
    static constexpr float sourceBumperFieldOfView=1.070468544960022f;
private:
    original::OriginalFscaTable trig_;
    Vec3 angles_{},previousAnchor_{},localOffset_{};
    OriginalChaseFrame frame_;
    bool ready_=false;
    OriginalDrivingView view_=OriginalDrivingView::Chase;
};
}
