#include "original_chase_camera.h"
#include <bit>
#include <cmath>
#include <stdexcept>
namespace idas3 {
namespace {
float word(std::uint32_t bits){return std::bit_cast<float>(bits);}
float filtered(float target,float previous,float response){
    //09D100 coefficients, then09C960 scalar operation order.
    const float a=1.f/(response+1.f),b=1.f-response;
    const float old=previous*b;float value=target+target;value-=old;value*=a;return value;
}
float shortArcPrevious(float previous,float target){
    const float difference=previous-target;
    if(std::fabs(difference)>word(0x40490fdbu))previous+=word(difference<0?0x40c90fdbu:0xc0c90fdbu);
    return previous;
}
}
OriginalChaseCamera OriginalChaseCamera::load(const std::filesystem::path& root,OriginalDrivingView view){OriginalChaseCamera out;out.view_=view;out.trig_=original::OriginalFscaTable::load(root/"data/original_physics/fsca_table.bin");return out;}
const OriginalChaseFrame& OriginalChaseCamera::update(Vec3 actor,Vec3 targetAngles){
    for(const float value:{actor.x,actor.y,actor.z,targetAngles.x,targetAngles.y,targetAngles.z})if(!std::isfinite(value))throw std::runtime_error("Non-finite original camera input");
    const Vec3 anchor{actor.x,actor.y-word(0x3ca3d70au),actor.z};
    const bool bumper=view_==OriginalDrivingView::Bumper;
    const float response=bumper?1.f:30.f;
    const Vec3 local=bumper?Vec3{0.f,1.f,-1.f}:Vec3{0.f,word(0x3fd9999au),word(0x40933333u)};
    if(!ready_){angles_=targetAngles;previousAnchor_=anchor;localOffset_=local;ready_=true;}
    localOffset_={filtered(local.x,localOffset_.x,response),filtered(local.y,localOffset_.y,response),filtered(local.z,localOffset_.z,response)};
    angles_={shortArcPrevious(angles_.x,targetAngles.x),shortArcPrevious(angles_.y,targetAngles.y),shortArcPrevious(angles_.z,targetAngles.z)};
    float movement=std::fabs(previousAnchor_.x-anchor.x);movement+=std::fabs(previousAnchor_.y-anchor.y);movement+=std::fabs(previousAnchor_.z-anchor.z);
    if(movement>10.f)angles_=targetAngles;
    angles_={filtered(targetAngles.x,angles_.x,response),filtered(targetAngles.y,angles_.y,response),filtered(targetAngles.z,angles_.z,response)};
    previousAnchor_=anchor;
    //09DA80: T(world anchor), RY, RX, RZ, T(filtered local offset).
    auto m=original::originalActorMatrix({anchor.x,anchor.y,anchor.z},{angles_.x,angles_.y,angles_.z},trig_);
    original::translateOriginalMatrix(m,{localOffset_.x,localOffset_.y,localOffset_.z});
    frame_.cameraWorld=m;const auto& v=m.elements;
    frame_.eye={v[12],v[13],v[14]};frame_.target={v[12]-v[8],v[13]-v[9],v[14]-v[10]};frame_.up={v[4],v[5],v[6]};
    //09D100 rounds the original full angle to integer phase for1D09E0.
    float phase=(bumper?sourceBumperFieldOfView:sourceVerticalFieldOfView)*65536.f;phase/=word(0x40c90fdbu);phase+=.5f;
    frame_.verticalFieldOfView=float(std::uint16_t(std::int32_t(phase)))*word(0x40c90fdbu)/65536.f;
    return frame_;
}
}
