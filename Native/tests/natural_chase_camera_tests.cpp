#include "natural_chase_camera.h"
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace idas3;
unsigned checks=0;
void require(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
bool close(float a,float b,float tolerance=1e-5f){return std::abs(a-b)<=tolerance;}
bool close(Vec3 a,Vec3 b,float tolerance=1e-5f){return length(a-b)<=tolerance;}
void finite(const NaturalChaseFrame& frame){
    for(float value:{frame.eye.x,frame.eye.y,frame.eye.z,frame.target.x,frame.target.y,frame.target.z,frame.verticalFieldOfView})
        require(std::isfinite(value),"Camera produced a non-finite frame");
    require(close(frame.up,{0,1,0}),"Natural horizon acquired roll");
    require(length(frame.target-frame.eye)>8.f,"Camera look direction collapsed");
    require(frame.verticalFieldOfView>1.f&&frame.verticalFieldOfView<1.11f,"FOV escaped its modest range");
}
void same(const NaturalChaseFrame& a,const NaturalChaseFrame& b,const char* message){
    require(close(a.eye,b.eye)&&close(a.target,b.target)&&close(a.up,b.up)&&close(a.verticalFieldOfView,b.verticalFieldOfView),message);
}
struct Pose {Vec3 car;float yaw,pitch,speed;};
Pose drive(double time){
    // A climbing, turning road that crosses the angular branch cut repeatedly.
    const float t=float(time),angle=2.9f+t*.72f+.13f*std::sin(t*.8f);
    return {{65.f*std::sin(angle),3.f*std::sin(t*.45f),65.f*std::cos(angle)},
            wrapAngle(angle+pi*.5f),.14f*std::sin(t*.45f),160.f+20.f*std::sin(t*.6f)};
}
std::array<NaturalChaseFrame,12> run(unsigned rate){
    NaturalChaseCamera camera;auto pose=drive(0);
    camera.update(pose.car,pose.yaw,pose.pitch,pose.speed,0);
    std::array<NaturalChaseFrame,12> frames;
    for(unsigned i=1;i<=rate*12;++i){
        pose=drive(double(i)/rate);
        const auto frame=camera.update(pose.car,pose.yaw,pose.pitch,pose.speed,1.0/rate);
        finite(frame);
        if(i%rate==0)frames[i/rate-1]=frame;
    }
    return frames;
}
}

int main()try{
    using namespace idas3;
    NaturalChaseCamera camera;
    require(!camera.ready(),"Camera starts initialized");
    const auto parked=camera.update({10,2,30},0,0,0,0);
    require(camera.ready(),"First zero-time frame did not initialize");finite(parked);
    require(parked.eye.z<30&&parked.target.z>30&&parked.eye.y>2,"Camera is not behind and above the car");
    for(unsigned i=0;i<100;++i)same(camera.update({10,2,30},0,0,0,1.0/60),parked,"Stationary camera drifts");
    same(camera.update({500,80,-500},2,.25f,280,0),parked,"Pause changed the ready camera");
    same(camera.update({500,80,-500},2,.25f,280,-1),parked,"Negative interval advanced camera");
    NaturalChaseCamera fresh;
    const auto teleported=camera.update({500,80,-500},2,.25f,280,1.0/60);
    same(teleported,fresh.update({500,80,-500},2,.25f,280,0),"Teleport retained old camera lag");
    camera.reset();require(!camera.ready(),"Reset retained readiness");
    fresh.reset();same(camera.update({-9,-2,7},-1,-.15f,90,0),fresh.update({-9,-2,7},-1,-.15f,90,0),"Reset differs from fresh camera");
    fresh.reset();same(camera.update({1,2,3},1,.2f,100,2),fresh.update({1,2,3},1,.2f,100,0),"Long frame carried stale camera lag");

    camera.reset();camera.update({},pi-.02f,0,100,0);
    const auto wrapped=camera.update({},-pi+.02f,0,100,1.0/60);
    require(wrapped.eye.z>5.f&&std::abs(wrapped.eye.x)<.3f,"Yaw wrap spun around the long arc");
    camera.reset();camera.update({},0,0,0,0);
    const auto turning=camera.update({},.2f,0,0,1.0/60);
    const float heading=std::atan2(turning.target.x-turning.eye.x,turning.target.z-turning.eye.z);
    require(heading>0&&heading<.1f,"Turn heading is not smoothly eased");
    for(float pitch:{-100.f,100.f}){
        camera.reset();const auto frame=camera.update({},0,pitch,1e6f,0);finite(frame);
        const auto look=normalized(frame.target-frame.eye);
        require(std::abs(look.y)<.4f,"Road pitch tilts the view excessively");
    }

    // High speed must never leave the car far ahead of a delayed camera.
    camera.reset();camera.update({},0,0,300,0);
    for(unsigned i=1;i<=600;++i){
        Vec3 car{0,float(i)*.03f,float(i)*(300.f/3.6f/60.f)};
        const auto frame=camera.update(car,0,.02f,300,1.0/60);finite(frame);
        require(car.z-frame.eye.z>5.f&&car.z-frame.eye.z<7.1f,"High-speed following lag is unbounded");
        require(std::abs(frame.eye.x-car.x)<.001f,"Straight driving introduces lateral sway");
    }
    const auto at30=run(30),at60=run(60),at120=run(120);
    float worst=0;
    for(unsigned i=0;i<at30.size();++i)for(const auto* sample:{&at30[i],&at60[i]}){
        worst=std::max(worst,std::max(length(sample->eye-at120[i].eye),length(sample->target-at120[i].target)));
        require(close(sample->eye,at120[i].eye,.035f)&&close(sample->target,at120[i].target,.035f),"Moving/turning response differs across frame rates");
        require(close(sample->verticalFieldOfView,at120[i].verticalFieldOfView,.0001f),"Speed FOV differs across frame rates");
    }
    const auto beforeInvalid=camera.frame();
    const float nan=std::numeric_limits<float>::quiet_NaN(),inf=std::numeric_limits<float>::infinity();
    for(unsigned field=0;field<7;++field){
        bool rejected=false;
        try{camera.update({field==0?nan:0,field==1?inf:0,field==2?nan:0},field==3?inf:0,field==4?nan:0,field==5?inf:0,field==6?std::numeric_limits<double>::infinity():.016);}
        catch(const std::invalid_argument&){rejected=true;}
        require(rejected,"Non-finite input was accepted");same(camera.frame(),beforeInvalid,"Rejected input corrupted the last valid frame");
    }
    std::cout<<"PASS "<<checks<<" natural camera checks; worst 30/60/120 Hz frame difference "<<worst<<" metres\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL: "<<error.what()<<'\n';return 1;}
