#include "original_start_camera.h"
#include <bit>
#include <cmath>
#include <stdexcept>

namespace idas3::original {
namespace {
#include "original_start_camera_data.inc"
float real(std::uint32_t word) { return std::bit_cast<float>(word); }
}

OriginalStartCameraShot originalStartCamera(std::uint32_t condition, std::uint32_t shot) {
    if (condition >= 18 || shot > 1)
        throw std::invalid_argument("Original start camera takes condition 0..17 and shot 0/1");
    const auto& words = originalStartCameraWords[condition * 2 + shot];
    OriginalStartCameraShot out;
    out.kind = words[0];
    out.smoothJoint = words[1];
    out.isShake = words[2];
    for (std::size_t i = 0; i < 3; ++i) out.position[i] = real(words[3 + i]);
    out.angleRadians = real(words[6]);
    out.wait = real(words[7]);
    for (std::size_t i = 0; i < 3; ++i) out.rotation[i] = real(words[8 + i]);
    for (std::size_t i = 0; i < 3; ++i) out.shake[i] = real(words[11 + i]);
    for (std::size_t i = 0; i < 3; ++i) out.offset[i] = real(words[14 + i]);
    out.objectSize = real(words[17]);
    return out;
}

namespace {
float sourceLength(const std::array<float,3>& v){
    // FIPR followed by FSQRT, matching the existing bounded scalar oracle.
    double sum=double(v[0])*v[0];sum+=double(v[1])*v[1];sum+=double(v[2])*v[2];
    return std::sqrt(float(sum));
}
}
void OriginalStartShowcaseCamera::reset(const std::array<float,3>& midpoint,
    float heading,unsigned course,bool reverse,const OriginalFscaTable& trig){
    if(course>8||!std::isfinite(heading))throw std::invalid_argument("Original showcase setup");
    for(float v:midpoint)if(!std::isfinite(v))throw std::invalid_argument("Original showcase position");
    nodes_={};selected_=0;ready_=false;
    auto target=midpoint;target[1]+=.5f; //0A8CB4
    const bool shomaruOutbound=course==6&&!reverse;
    if(shomaruOutbound)target[1]+=1.f; //0A8CC8
    auto matrix=originalIdentityMatrix();translateOriginalMatrix(matrix,target);
    rotateOriginalMatrixY(matrix,heading,trig);
    //06455C supplies zero pitch/roll. Keep the original three-rotation order.
    rotateOriginalMatrixX(matrix,0.f,trig);rotateOriginalMatrixZ(matrix,0.f,trig);
    for(unsigned shot=0;shot<2;++shot){
        auto& n=nodes_[shot];
        // Authored0C24AF14/0C24AF48, 13 words each. The constructor replaces
        // target, velocity and the authored45-tick hold with owner120.
        std::array<float,3> local{shot?2.f:-2.f,real(0x3f333333),shot?-6.f:6.f};
        if(shomaruOutbound)local[0]*=.5f;
        n.frame.target=target;n.frame.eye=transformOriginalPoint(matrix,local);
        n.frame.verticalFieldOfView=real(0x3f860a92);
        float speed=-local[0];speed+=speed;speed/=float(sourceActiveTicks);
        for(unsigned axis=0;axis<3;++axis)n.authoredVelocity[axis]=matrix.elements[axis]*speed;
        n.maximumSpeed=sourceLength(n.authoredVelocity);
        n.remaining=sourceActiveTicks;
    }
    ready_=true;
}
void OriginalStartShowcaseCamera::selectShot(unsigned shot){
    if(!ready_||shot>1)throw std::invalid_argument("Original showcase shot");
    selected_=shot;
}
void OriginalStartShowcaseCamera::advance(bool slow){
    if(!ready_)throw std::logic_error("Original showcase was not initialized");
    auto& n=nodes_[selected_];float speed=sourceLength(n.velocity);
    const float acceleration=real(slow?0x3a83126f:0x3c23d70b);
    bool move=false,resize=false;
    if(n.remaining){
        --n.remaining;move=true;
        if(n.maximumSpeed>speed){speed+=acceleration;resize=true;}
    }else if(speed>real(0x3cf5c28f)){
        speed-=acceleration;move=resize=true;
    }
    if(resize){
        //1F6C40 computes FSRRA(lengthSquared)*requestedLength, then three
        // component multiplies. It deliberately does not clamp overshoot.
        double sum=double(n.authoredVelocity[0])*n.authoredVelocity[0];
        sum+=double(n.authoredVelocity[1])*n.authoredVelocity[1];
        sum+=double(n.authoredVelocity[2])*n.authoredVelocity[2];
        const float inverse=1.f/std::sqrt(float(sum));
        const float factor=inverse*speed;
        for(unsigned axis=0;axis<3;++axis)n.velocity[axis]=n.authoredVelocity[axis]*factor;
    }
    if(move)for(unsigned axis=0;axis<3;++axis)n.frame.eye[axis]+=n.velocity[axis];
    ++n.updates;
}
}
