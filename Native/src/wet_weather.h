#pragma once
#include "math_types.h"
#include <array>
#include <cstdint>

namespace idas3 {
// Host presentation adapter using the original effect/rain streak and effect/rainmark water trail textures.
// Particle placement/timing is a bounded host implementation, not a recovered
// SH4 owner. Its private deterministic RNG never touches the driving RNG.
class WetWeather {
public:
    static constexpr unsigned rainCount=256,sprayCapacity=96;
    struct Car { Vec3 position{};float yaw=0,speed=0;bool visible=false;std::array<Vec3,2> rearContacts{};bool contactsValid=false; };
    struct Spray { Vec3 position{},velocity{},direction{};float age=0,life=0,size=0,extent=0; };
    struct Quad { Vec3 center{},across{},up{};float alpha=0;unsigned texture=0;bool waterTrail=false; };
    std::array<Quad,rainCount+sprayCapacity> quads{};
    unsigned count=0;
    void reset(){*this=WetWeather{};}
    void advance(double dt,bool enabled,bool paused,const std::array<Car,2>& cars) {
        if(!enabled){if(ticks||count)reset();return;}
        if(paused)return;
        remainder+=std::clamp(dt,0.,.25);
        while(remainder+1e-9>=1./60.) {
            remainder-=1./60.;++ticks;
            for(auto& p:spray)if(p.life>0) {
                p.age+=1.f/60.f;
                if(p.age>=p.life){p.life=0;continue;}
                p.position+=p.velocity*(1.f/60.f);p.velocity*=.975f;
            }
            for(unsigned c=0;c<2;++c) {
                const auto& car=cars[c];
                // Clear trails across respawns/remote teleports. Do not connect
                // the old location to a new starting grid or another course.
                if(!car.visible||(seen[c]&&length(car.position-last[c])>25.f))
                    for(unsigned i=c*48;i<(c+1)*48;++i)spray[i].life=0;
                last[c]=car.position;seen[c]=car.visible;
                if(!car.visible||car.speed<2.f||ticks%3!=0)continue;
                const float strength=std::clamp((car.speed-2.f)/24.f,0.f,1.f);
                for(int side:{-1,1}) {
                    auto& p=spray[c*48+(cursor[c]++%48)];
                    const auto f=forward(car.yaw),r=right(car.yaw);
                    p.position=(car.contactsValid?car.rearContacts[side<0?0:1]:car.position-f*1.25f+r*(float(side)*.72f))+Vec3{0,.12f,0};
                    p.velocity=f*(car.speed*.12f)+r*(float(side)*(.3f+random()*.5f))+Vec3{0,.20f+random()*.15f,0};
                    p.direction=f;p.extent=.6f+strength*1.2f;
                    p.age=0;p.life=.18f+strength*.15f;p.size=.22f+strength*.20f;
                }
            }
        }
    }
    void build(Vec3 eye,Vec3 target,bool enabled,unsigned stride=1) {
        count=0;if(!enabled)return;
        stride=std::clamp(stride,1u,4u);
        const auto view=normalized(target-eye);
        auto horizontal=normalized(cross(view,{0,1,0}));
        if(std::abs(view.y)>.99f)horizontal={1,0,0};
        
        const float time=float(ticks%36000)/60.f;
        // A periodic world-space field follows the camera's visible volume.
        // Camera translation doesn't drag the drops along with the car.
        // Keep the rear hemisphere too: the mirror renders these same ranges.
        for(unsigned i=0;i<rainCount;i+=stride) {
            const Vec3 origin{hash(i*3+1)*28.f,hash(i*3+2)*16.f,hash(i*3+3)*36.f};
            const Vec3 p{eye.x+wrap(origin.x+time*.8f-eye.x,28.f)-14.f,
                eye.y+wrap(origin.y-time*24.f-eye.y,16.f)-8.f,
                eye.z+wrap(origin.z+time*.3f-eye.z,36.f)-18.f};
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
            const float size=p.size*(1.f+t*.65f);
            if(length(p.position-eye)<.6f)continue;
            // Long, low water trails follow the rear wheel direction instead of
            // camera-facing powder puffs. The source rainmark single-track
            // texture is applied independently to each actual tire contact.
            quads[count++]={p.position-p.direction*p.extent+Vec3{0,.20f+t*.18f,0},
                cross(Vec3{0,1,0},p.direction)*size,
                p.direction*p.extent+Vec3{0,-.20f,0},.7f*(1.f-t),1,true};
        }
    }
    unsigned liveSpray()const {unsigned n=0;for(const auto& p:spray)if(p.life>0)++n;return n;}
    std::uint64_t frame()const{return ticks;}
private:
    std::array<Spray,sprayCapacity> spray{};
    std::array<unsigned,2> cursor{};
    std::array<Vec3,2> last{};std::array<bool,2> seen{};
    std::uint64_t ticks=0;double remainder=0;std::uint32_t seed=0x73821u;
    static float wrap(float v,float span){return v-std::floor(v/span)*span;}
    static float hash(std::uint32_t x){x^=x>>16;x*=0x7feb352du;x^=x>>15;x*=0x846ca68bu;x^=x>>16;return float(x&0xffffffu)/16777216.f;}
    float random(){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;}
};
}
