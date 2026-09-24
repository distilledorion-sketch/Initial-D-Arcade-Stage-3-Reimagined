#pragma once
#include "math_types.h"
#include <stdexcept>

namespace idas3 {
struct NaturalChaseFrame {
    Vec3 eye{},target{},up{0,1,0};
    float verticalFieldOfView=58.f*pi/180.f;
};

// Optional host presentation only. Position uses metres, heading is forward(yaw),
// positive road pitch points uphill, and speed is km/h. No body roll or shake is
// fed into this camera; the source driving and original camera owners are separate.
class NaturalChaseCamera {
public:
    void reset(){ready_=false;}
    bool ready()const{return ready_;}
    const NaturalChaseFrame& frame()const{return frame_;}
    const NaturalChaseFrame& update(Vec3 car,float yaw,float roadPitch,float speed,double seconds){
        for(float value:{car.x,car.y,car.z,yaw,roadPitch,speed})
            if(!std::isfinite(value))throw std::invalid_argument("Non-finite natural camera input");
        if(!std::isfinite(seconds))throw std::invalid_argument("Non-finite natural camera interval");
        // A paused ready camera is a frozen presentation, even if its caller's
        // interpolation supplies a different pose. Its last live input stays put.
        if(ready_&&seconds<=0)return frame_;
        const float heading=wrapAngle(yaw),pitch=std::clamp(roadPitch,-.3f,.3f);
        const float kmh=std::clamp(std::abs(speed),0.f,300.f);
        const double dx=double(car.x)-lastCar_.x,dy=double(car.y)-lastCar_.y,dz=double(car.z)-lastCar_.z;
        const double expectedTravel=std::max(kmh,lastSpeed_)/3.6*std::max(seconds,0.0);
        const double teleportDistance=std::max(12.0,expectedTravel*2.0+2.0);
        if(!ready_||seconds>.25||dx*dx+dy*dy+dz*dz>teleportDistance*teleportDistance){
            anchor_=car;yaw_=heading;pitch_=pitch;speed_=kmh;ready_=true;
        }else{
            anchor_.x=follow(anchor_.x,lastCar_.x,car.x,18.0,seconds);
            anchor_.z=follow(anchor_.z,lastCar_.z,car.z,18.0,seconds);
            anchor_.y=follow(anchor_.y,lastCar_.y,car.y,7.5,seconds);
            Vec3 lag{anchor_.x-car.x,0,anchor_.z-car.z};
            const float lagLength=length(lag);
            if(lagLength>.65f){lag*=.65f/lagLength;anchor_.x=car.x+lag.x;anchor_.z=car.z+lag.z;}
            anchor_.y=car.y+std::clamp(anchor_.y-car.y,-.35f,.35f);
            // Integrate the short arc across +/-pi, then cap angular lag so a
            // hairpin never leaves the camera looking at the car from its side.
            const float nextYaw=lastYaw_+wrapAngle(heading-lastYaw_);
            const float oldYaw=lastYaw_+wrapAngle(yaw_-lastYaw_);
            yaw_=follow(oldYaw,lastYaw_,nextYaw,10.0,seconds);
            yaw_=wrapAngle(nextYaw+std::clamp(yaw_-nextYaw,-.24f,.24f));
            pitch_=follow(pitch_,lastPitch_,pitch,5.0,seconds);
            speed_=follow(speed_,lastSpeed_,kmh,3.5,seconds);
        }
        lastCar_=car;lastYaw_=heading;lastPitch_=pitch;lastSpeed_=kmh;
        const float pace=std::clamp(speed_/200.f,0.f,1.f);
        const float distance=5.4f+.9f*pace,lookAhead=4.8f+2.5f*pace;
        const Vec3 facing=forward(yaw_)*std::cos(pitch_)+Vec3{0,std::sin(pitch_),0};
        frame_.eye=anchor_-facing*distance+Vec3{0,1.65f,0};
        frame_.target=anchor_+facing*lookAhead+Vec3{0,.9f,0};
        frame_.up={0,1,0};frame_.verticalFieldOfView=(58.f+5.f*pace)*pi/180.f;
        return frame_;
    }
private:
    // Exact first-order response to a linearly moving target over this interval.
    // Unlike repeatedly easing toward the end pose, this preserves the same
    // lag at 30/60/120 Hz while the car travels and turns between samples.
    static float follow(float value,float previous,float target,double rate,double seconds){
        const double step=rate*seconds,weight=-std::expm1(-step);
        const double ramp=step<1e-5?step*(.5-step/6.0+step*step/24.0):1.0-weight/step;
        return float(double(value)+(double(previous)-value)*weight+(double(target)-previous)*ramp);
    }
    bool ready_=false;
    Vec3 anchor_{},lastCar_{};
    float yaw_=0,lastYaw_=0,pitch_=0,lastPitch_=0,speed_=0,lastSpeed_=0;
    NaturalChaseFrame frame_{};
};
}
