#pragma once
#include "math_types.h"
#include "original_matrix.h"
#include <bit>
#include <cmath>
#include <cstdint>

namespace idas3 {
// Kind13 (0A34E0): the eye follows its authored car-relative point while
// 09C9C0 applies the authored spring/noise to the look-at point. Run once per
// original tick, not per render. Own the presentation RNG so credits cannot
// mutate the completed race or its rollback/save state.
class OriginalEndingCamera {
public:
    static constexpr Vec3 eyeOffset{std::bit_cast<float>(0x3fdd709cu),
        std::bit_cast<float>(0x40a0a402u),std::bit_cast<float>(0xc1142970u)};
    static constexpr Vec3 targetOffset{std::bit_cast<float>(0x3e6147a4u),
        std::bit_cast<float>(0x4035c286u),std::bit_cast<float>(0x3cf5c2a7u)};
    static constexpr float authoredFieldOfView=std::bit_cast<float>(0x3f860a92u);
    void reset(Vec3 eye,Vec3 target,std::uint32_t seed){
        eye_=eye;target_=target;velocity_={};seed_=seed;updates_=0;
    }
    void advance(Vec3 eye,Vec3 requestedTarget){
        constexpr float acceleration=std::bit_cast<float>(0x3eb33331u);
        constexpr float convergence=std::bit_cast<float>(0x3eb33331u);
        constexpr float noise=std::bit_cast<float>(0x3ea8f5c1u);
        const float desired[3]{requestedTarget.x,requestedTarget.y,requestedTarget.z};
        float previous[3]{target_.x,target_.y,target_.z};
        float velocity[3]{velocity_.x,velocity_.y,velocity_.z};
        for(unsigned axis=0;axis<3;++axis){
            seed_=seed_*0x41c64e6du+0x3039u;
            const float random=float((seed_>>16)&0x7fffu)*std::bit_cast<float>(0x38000000u);
            const float disturbed=std::fma(random-.5f,noise,desired[axis]+velocity[axis]);
            velocity[axis]=std::fma(disturbed-previous[axis],acceleration,velocity[axis]);
            velocity[axis]*=convergence;
            previous[axis]+=velocity[axis];
        }
        eye_=eye;target_={previous[0],previous[1],previous[2]};
        velocity_={velocity[0],velocity[1],velocity[2]};++updates_;
    }
    Vec3 eye()const{return eye_;}
    void advanceShared(Vec3 eye,Vec3 target,std::uint32_t& random){
        seed_=random;advance(eye,target);random=seed_;
    }
    Vec3 target()const{return target_;}
    Vec3 velocity()const{return velocity_;}
    std::uint32_t seed()const{return seed_;}
    unsigned updates()const{return updates_;}
    original::OriginalMatrix worldToView()const{
        return original::originalCameraWorldToView(original::originalLookAtCameraWorld(
            {eye_.x,eye_.y,eye_.z},{target_.x,target_.y,target_.z}));
    }
    static std::uint16_t fieldOfViewPhase(){
        //09D100 rounds the authored radians to the original projection phase.
        constexpr float turn=std::bit_cast<float>(0x40c90fdbu);
        float phase=authoredFieldOfView*65536.f;phase/=turn;phase+=.5f;
        return std::uint16_t(std::int32_t(phase));
    }
    static float fieldOfView(){
        return float(fieldOfViewPhase())*std::bit_cast<float>(0x40c90fdbu)/65536.f;
    }
private:
    Vec3 eye_{},target_{},velocity_{};
    std::uint32_t seed_=1;
    unsigned updates_=0;
};
}
