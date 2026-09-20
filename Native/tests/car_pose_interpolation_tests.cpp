#include "car_pose_interpolation.h"
#include "car_wheel_pose.h"
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
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
