#pragma once
#include "math_types.h"
#include "weather_shelter.h"
#include <array>
#include <cstdint>

namespace idas3 {
// Host presentation adapter using original effect/rain flakes, powder and
// streaks, plus effect/rainmark water trails. Snow and rain remain distinct.
// Particle placement/timing is a bounded host implementation, not a recovered
// SH4 owner. Its private deterministic RNG never touches the driving RNG.
class WetWeather {
public:
    static constexpr unsigned rainCount=256,sprayCapacity=96;
    struct Car {
        Vec3 position{};float yaw=0,speed=0;bool visible=false;
        std::array<Vec3,2> rearContacts{};bool contactsValid=false;
        std::array<Vec3,2> rearNormals{{{0,1,0},{0,1,0}}};
    };
    struct Spray { Vec3 position{},velocity{},direction{};float age=0,life=0,size=0,extent=0;Vec3 normal{0,1,0}; };
    struct Quad { Vec3 center{},across{},up{};float alpha=0;unsigned texture=0;bool waterTrail=false; };
    std::array<Quad,rainCount+sprayCapacity> quads{};
    unsigned count=0;
    void reset(){*this=WetWeather{};}
    void advance(double dt,bool enabled,bool paused,const std::array<Car,2>& cars,bool snow=false) {
        if(!enabled){if(ticks||count)reset();return;}
        if(snowMode!=snow){reset();snowMode=snow;}
        if(paused||!std::isfinite(dt))return;
        remainder+=std::clamp(dt,0.,.25);
        while(remainder+1e-9>=1./60.) {
            remainder-=1./60.;++ticks;
            for(auto& p:spray)if(p.life>0) {
                p.age+=1.f/60.f;
                if(p.age>=p.life){p.life=0;continue;}
                // Rainmarks are deposited on the road. Only snow powder has
                // airborne motion after leaving the tire.
                if(snowMode){p.position+=p.velocity*(1.f/60.f);p.velocity*=.975f;p.velocity.y-=.45f/60.f;}
            }
            for(unsigned c=0;c<2;++c) {
                const auto& car=cars[c];
                // Clear trails across respawns/remote teleports. Do not connect
                // the old location to a new starting grid or another course.
                if(!car.visible||(seen[c]&&length(car.position-last[c])>25.f)){
                    for(unsigned i=c*48;i<(c+1)*48;++i)spray[i].life=0;
                    contactSeen[c]=false;
                }
                last[c]=car.position;seen[c]=car.visible;
                if(!snowMode){
                    // No horizontal fallback under airborne/missing contacts.
                    // Both tires must have a trustworthy road plane, retaining
                    // complete source left/right pairs at every detail level.
                    bool valid=car.contactsValid&&finite(car.position)&&std::isfinite(car.yaw)&&std::isfinite(car.speed);
                    for(unsigned side=0;side<2;++side)
                        valid=valid&&finite(car.rearContacts[side])&&finite(car.rearNormals[side])&&
                            length(car.rearNormals[side])>.5f&&normalized(car.rearNormals[side]).y>.2f;
                    if(!valid){contactSeen[c]=false;continue;}
                    for(unsigned side=0;side<2;++side){
                        const auto normal=normalized(car.rearNormals[side]);
                        const auto motion=car.rearContacts[side]-lastContacts[c][side];
                        const auto tangent=motion-normal*dot(motion,normal);
                        if(contactSeen[c]&&length(tangent)>.001f&&length(motion)<25.f)
                            directions[c][side]=normalized(tangent);
                        else if(!contactSeen[c])directions[c][side]=forward(car.yaw);
                        auto along=directions[c][side]-normal*dot(directions[c][side],normal);
                        if(length(along)<.001f)along=forward(car.yaw)-normal*dot(forward(car.yaw),normal);
                        directions[c][side]=normalized(along);
                        lastContacts[c][side]=car.rearContacts[side];
                    }
                    contactSeen[c]=true;
                }
                if(!car.visible||car.speed<(snowMode?.8f:2.f)||ticks%3!=0)continue;
                const float strength=std::clamp((car.speed-2.f)/24.f,0.f,1.f);
                for(int side:{-1,1}) {
                    auto& p=spray[c*48+(cursor[c]++%48)];
                    const auto f=forward(car.yaw),r=right(car.yaw);
                    p.position=(car.contactsValid?car.rearContacts[side<0?0:1]:car.position-f*1.25f+r*(float(side)*.72f))+Vec3{0,.12f,0};
                    p.velocity=f*(car.speed*.12f)+r*(float(side)*(.3f+random()*.5f))+Vec3{0,.20f+random()*.15f,0};
                    p.direction=f;p.extent=.6f+strength*1.2f;
                    p.age=0;p.life=.18f+strength*.15f;p.size=.22f+strength*.20f;
                    if(snowMode){
                        p.velocity=f*(car.speed*.05f)+r*(float(side)*(.4f+random()*.5f))+Vec3{0,.65f+random()*.35f,0};
                        p.life=.45f+strength*.25f;p.size=.16f+strength*.18f;
                    }else{
                        const unsigned wheel=side<0?0:1;
                        p.normal=normalized(car.rearNormals[wheel]);
                        p.position=car.rearContacts[wheel]+p.normal*.012f;
                        p.velocity={};p.direction=directions[c][wheel];
                    }
                }
            }
        }
    }
    void build(Vec3 eye,Vec3 target,bool enabled,unsigned stride=1,const WeatherShelter* shelter=nullptr) {
        count=0;if(!enabled)return;
        stride=std::clamp(stride,1u,4u);
        const auto view=normalized(target-eye);
        auto horizontal=normalized(cross(view,{0,1,0}));
        if(std::abs(view.y)>.99f)horizontal={1,0,0};
        const auto vertical=normalized(cross(horizontal,view));
        
        const float time=float(ticks%36000)/60.f;
        // A periodic world-space field follows the camera's visible volume.
        // Camera translation doesn't drag the drops along with the car.
        // Keep the rear hemisphere too: the mirror renders these same ranges.
        for(unsigned i=0;i<rainCount;i+=stride) {
            if(snowMode){
                // Slow drifting flakes, not elongated rain streaks. Reuse the
                // original soft particle and powdered-snow textures (1 and 7).
                const float phase=hash(i+1001)*6.2831853f;
                const Vec3 p{eye.x+wrap(hash(i*3+1)*20.f+time*.45f+std::sin(time*.7f+phase)*.4f-eye.x,20.f)-10.f,
                    eye.y+wrap(hash(i*3+2)*10.f-time*(1.1f+hash(i+2001)*.7f)-eye.y,10.f)-5.f,
                    eye.z+wrap(hash(i*3+3)*24.f+time*.18f+std::cos(time*.5f+phase)*.4f-eye.z,24.f)-12.f};
                if(shelter&&shelter->covered(p))continue;
                const float depth=std::abs(dot(p-eye,view));
                if(depth<.6f)continue;
                const float size=.025f+hash(i+3001)*.035f;
                quads[count++]={p,horizontal*size,vertical*size,.8f*std::clamp((depth-.6f)/1.5f,0.f,1.f),1};
                continue;
            }
            const Vec3 origin{hash(i*3+1)*28.f,hash(i*3+2)*16.f,hash(i*3+3)*36.f};
            const Vec3 p{eye.x+wrap(origin.x+time*.8f-eye.x,28.f)-14.f,
                eye.y+wrap(origin.y-time*24.f-eye.y,16.f)-8.f,
                eye.z+wrap(origin.z+time*.3f-eye.z,36.f)-18.f};
            // Test the bottom of the streak so it cannot cross a ceiling.
            if(shelter&&shelter->covered(p-Vec3{0,.9f,0}))continue;
            const auto d=p-eye;const float depth=std::abs(dot(d,view));
            if(depth<1.5f)continue;
            const float fade=std::clamp((depth-1.5f)/3.f,0.f,1.f);
            quads[count++]={p,horizontal*.06f,Vec3{-.02f,.9f,-.01f},.35f*fade,6};
        }
        // Sort the small spray pool back-to-front; rain and spray each batch
        // into one range, so there is no per-particle object/material cost.
        std::array<unsigned,sprayCapacity> order{};unsigned active=0;
        // Keep complete left/right wheel pairs when reducing the trail pool.
        for(unsigned i=0;i<sprayCapacity;++i)if((i/2)%stride==0&&spray[i].life>0)order[active++]=i;
        std::sort(order.begin(),order.begin()+active,[&](unsigned a,unsigned b){return dot(spray[a].position-eye,view)>dot(spray[b].position-eye,view);});
        for(unsigned j=0;j<active;++j) {
            const auto& p=spray[order[j]];const float t=p.age/p.life;
            if(length(p.position-eye)<.6f)continue;
            if(snowMode){
                const float powderSize=p.size*(1.f+t*1.8f);
                quads[count++]={p.position,horizontal*powderSize,vertical*powderSize,.65f*(1.f-t),7};
                continue;
            }
            // The whole rainmark lies on the sampled tire contact plane. Its
            // travel tangent includes a slide across the road, independent of
            // body yaw, camera, later steering and particle age.
            quads[count++]={p.position-p.direction*p.extent,
                cross(p.normal,p.direction)*p.size,
                p.direction*p.extent,.7f*(1.f-t),1,true};
        }
    }
    unsigned liveSpray()const {unsigned n=0;for(const auto& p:spray)if(p.life>0)++n;return n;}
    std::uint64_t frame()const{return ticks;}
private:
    std::array<Spray,sprayCapacity> spray{};
    std::array<unsigned,2> cursor{};
    std::array<Vec3,2> last{};std::array<bool,2> seen{};
    std::array<std::array<Vec3,2>,2> lastContacts{},directions{};
    std::array<bool,2> contactSeen{};
    std::uint64_t ticks=0;double remainder=0;std::uint32_t seed=0x73821u;
    bool snowMode=false;
    static bool finite(Vec3 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z);}
    static float wrap(float v,float span){return v-std::floor(v/span)*span;}
    static float hash(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return float(x&0xffffffu)/16777216.f;}
    float random(){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;}
};
}
