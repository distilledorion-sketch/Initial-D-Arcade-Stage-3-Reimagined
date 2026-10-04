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
    OriginalRearViewFrame rearView(Vec3 actorPosition,Vec3 actorAngles)const{
        // In bumper view the local body draw (034C20) is absent. ABackView
        // therefore sees 034840's actor matrix, before the body-only lift.
        // Keep the host rear-facing basis; the source model's half-turn is
        // already accounted for by the mirrored, left-handed projection.
        actorPosition.y-=std::bit_cast<float>(0x3ca3d70au);
        return originalRearViewFrame(original::originalActorMatrix(
            {actorPosition.x,actorPosition.y,actorPosition.z},
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
