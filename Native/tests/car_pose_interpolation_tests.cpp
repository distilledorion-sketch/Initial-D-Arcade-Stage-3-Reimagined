#include "car_pose_interpolation.h"
#include "car_wheel_pose.h"
#include "online_visual_correction.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
int main()try{
    unsigned checks=0;
    for(float direction:{-1.f,1.f})for(bool reverse:{false,true}){
        CarBodyAngles a{direction*.001f,direction*(2*pi-.002f)},b{direction*(2*pi-.001f),direction*.002f};
        if(reverse)std::swap(a,b);
        for(unsigned frame=0;frame<=100;++frame){
            const auto pose=interpolateCarBodyAngles(a,b,float(frame)/100);
            // Transform the chassis up direction with the renderer's RX*RZ
            // convention. Every frame of this tiny road-normal change must
            // remain upright; linear raw-angle interpolation fails this test.
            const Vec3 up{-std::sin(pose.roll),std::cos(pose.roll)*std::cos(pose.pitch),std::cos(pose.roll)*std::sin(pose.pitch)};
            if(up.y<.99999f||std::abs(up.x)>.00201f||std::abs(up.z)>.00101f)throw std::runtime_error("Car flips while body angles cross zero");
            ++checks;
        }
    }
    const auto realBank=interpolateCarBodyAngles({0,0},{pi/2,pi/2},.5f);
    if(std::abs(realBank.pitch-pi/4)>1e-6f||std::abs(realBank.roll-pi/4)>1e-6f)throw std::runtime_error("Valid body rotation was clamped");
    unsigned wheelChecks=0;
    for(bool reverse:{false,true}){
        CarWheelPose a,b;a.steeringRadians=.001f;b.steeringRadians=2*pi-.001f;
        a.suspensionY.fill(-.02f);b.suspensionY.fill(.02f);a.rotationRadians.fill(2*pi-.001f);b.rotationRadians.fill(.001f);
        if(reverse)std::swap(a,b);
        for(unsigned frame=0;frame<=100;++frame){const float alpha=float(frame)/100;const auto pose=interpolateCarWheels(a,b,alpha);
            if(std::cos(pose.steeringRadians)<.99999f)throw std::runtime_error("Front wheels swing sideways across rival steering wrap");
            for(unsigned i=0;i<4;++i)if(std::cos(pose.rotationRadians[i])<.99999f||std::abs(pose.suspensionY[i]-(a.suspensionY[i]+(b.suspensionY[i]-a.suspensionY[i])*alpha))>1e-7f)throw std::runtime_error("Wheel spin/suspension interpolation changed");
            ++wheelChecks;
        }
    }
    std::cout<<"PASS "<<wheelChecks<<" wrapped front-wheel steering poses retain direction and suspension/spin.\n";
    std::cout<<"PASS "<<checks<<" zero-crossing chassis poses remain upright; real pitch and roll preserved.\n";
    Vec3 correction{1,0,0},velocity{};
    advanceOnlineVisualCorrection(correction,velocity,1.f/60);
    const float firstCorrection=1-correction.x;
    if(firstCorrection>=.05f||firstCorrection<=0)throw std::runtime_error("Rollback starts with an abrupt catch-up");
    float previous=correction.x;
    for(unsigned frame=0;frame<120;++frame){
        advanceOnlineVisualCorrection(correction,velocity,1.f/60);
        if(correction.x<0||correction.x>previous)throw std::runtime_error("Isolated rollback oscillates");previous=correction.x;
    }
    if(length(correction)>1e-5f||length(velocity)>1e-4f)throw std::runtime_error("Correction never settles");
    Vec3 a{.7f,-.2f,.4f},av{-.3f,.1f,.8f},b=a,bv=av;
    for(unsigned i=0;i<30;++i)advanceOnlineVisualCorrection(a,av,1.f/60);
    for(unsigned i=0;i<60;++i)advanceOnlineVisualCorrection(b,bv,1.f/120);
    if(length(a-b)>1e-5f||length(av-bv)>1e-5f)throw std::runtime_error("Correction depends on display subdivision");
    // A newer rollback must preserve the displayed position at packet arrival
    // without restarting the velocity accumulated by earlier corrections.
    Vec3 raw{10,2,4},offset{.2f,.1f,0},v{-.3f,0,.1f};
    const Vec3 displayed=raw+offset,priorVelocity=v,delta{.5f,-.1f,.4f};raw=raw+delta;offset=offset-delta;
    if(length(raw+offset-displayed)>1e-5f||length(v-priorVelocity)>1e-7f)throw std::runtime_error("Packet changes the displayed pose/velocity immediately");
    float yaw=wrapAngle((pi-.01f)-(-pi+.01f)),yawVelocity=0;
    for(unsigned i=0;i<60;++i){advanceOnlineVisualCorrection(yaw,yawVelocity,1.f/60);if(std::abs(yaw)>.021f)throw std::runtime_error("Yaw correction takes the long turn");}
    std::cout<<"PASS rollback presentation: first tick="<<firstCorrection<<" versus legacy 0.125; monotonic settling, subdivision, packet continuity and wrapped yaw.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
