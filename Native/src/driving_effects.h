#pragma once
#include "renderer.h"
#include <array>

namespace idas3 {
// Bounded host presentation, independent of handling, race RNG and audio.
// Smoke uses coverage derived from recovered effect/smoke texture 1. Rubber ribbons are host
// geometry, not a claimed recovered arcade decal or effect controller.
class DrivingEffects {
public:
    struct Car {
        std::array<Vec3,4> points{},normals{};
        Vec3 position{};
        float yaw=0,speed=0,slip=0;
        bool grounded=false,visible=false;
    };
    static constexpr unsigned markCapacity=2048,smokeCapacity=128;
    void reset(){*this=DrivingEffects{};}
    void advance(double dt,bool enabled,bool paused,const std::array<Car,2>& cars) {
        if(!enabled){reset();return;}
        if(paused||!std::isfinite(dt))return;
        const float elapsed=float(std::clamp(dt,0.,.1));
        for(auto& m:marks_)m.life=(std::max)(0.f,m.life-elapsed);
        for(auto& p:smoke_)if(p.life>0){p.life-=elapsed;p.position+=p.velocity*elapsed;p.size+=elapsed*.65f;}
        for(unsigned c=0;c<2;++c){const auto& car=cars[c];
            const bool teleported=seen_[c]&&length(car.position-lastCar_[c])>8;
            if(teleported||!car.visible){for(auto& p:smoke_)if(p.owner==c)p.life=0;connected_[c]=false;}
            // Signed source slip and a kinematic slip estimate both reach here.
            const float strength=std::clamp((std::abs(car.slip)-.12f)/.32f,0.f,1.f);
            const bool active=car.visible&&car.grounded&&car.speed>5&&strength>0;
            lastCar_[c]=car.position;seen_[c]=car.visible;
            if(!active){connected_[c]=false;emission_[c]=0;continue;}
            emission_[c]+=elapsed;
            if(emission_[c]>=.05f){
                emission_[c]=std::fmod(emission_[c],.05f);
                for(unsigned side=0;side<2;++side){const unsigned w=side+2;
                    auto& p=smoke_[smokeCursor_++%smokeCapacity];
                    p={car.points[w]+car.normals[w]*.24f,forward(car.yaw)*(.06f*car.speed)+Vec3{0,.8f,0},.65f,.16f,strength,c};
                }
            }
            if(connected_[c]){
                const float distance=length(car.points[2]-lastPoints_[c][0]);
                if(distance<.16f)continue;
                if(distance<3.f)for(unsigned side=0;side<2;++side){const unsigned w=side+2;
                    const auto along=car.points[w]-lastPoints_[c][side];
                    const auto left=normalized(cross(lastNormals_[c][side],along))*.095f;
                    const auto right=normalized(cross(car.normals[w],along))*.095f;
                    const auto a=lastPoints_[c][side]+lastNormals_[c][side]*.012f;
                    const auto b=car.points[w]+car.normals[w]*.012f;
                    marks_[markCursor_++%markCapacity]={{a-left,a+left,b+right,b-right},20.f,strength};
                }
            }
            for(unsigned side=0;side<2;++side){lastPoints_[c][side]=car.points[side+2];lastNormals_[c][side]=car.normals[side+2];}
            connected_[c]=true;
        }
    }
    void append(Mesh& mesh,Vec3 eye,Vec3 target,std::uint32_t smokeTexture,bool night)const {
        constexpr unsigned tsp=(4u<<29)|(5u<<26)|(1u<<20)|(2u<<22),isp=(6u<<29)|(1u<<26),pcw=(2u<<24)|2u;
        const auto triangle=[&](Vertex a,Vertex b,Vertex c){mesh.vertices.insert(mesh.vertices.end(),{a,b,c});};
        unsigned first=unsigned(mesh.vertices.size());
        for(const auto& m:marks_)if(m.life>0&&length(m.points[0]-eye)<160){
            const float alpha=.36f*m.strength*(std::min)(m.life/3.f,1.f);
            const Color color{.025f,.025f,.025f,alpha};
            const auto n=normalized(cross(m.points[1]-m.points[0],m.points[3]-m.points[0]));
            const Vertex a{m.points[0],n,color},b{m.points[1],n,color},c{m.points[2],n,color},d{m.points[3],n,color};
            triangle(a,b,c);triangle(a,c,d);
        }
        if(mesh.vertices.size()>first)mesh.ranges.push_back({first,unsigned(mesh.vertices.size())-first,0xffffffffu,tsp,pcw,isp,512,true});
        if(smokeTexture==0xffffffffu)return;
        const auto view=normalized(target-eye);
        const auto across=std::abs(view.y)>.99f?Vec3{1,0,0}:normalized(cross(view,{0,1,0}));
        const auto up=normalized(cross(across,view));
        std::array<unsigned,smokeCapacity> order{};unsigned count=0;
        for(unsigned i=0;i<smokeCapacity;++i)if(smoke_[i].life>0&&length(smoke_[i].position-eye)>1.5f&&length(smoke_[i].position-eye)<100)order[count++]=i;
        std::sort(order.begin(),order.begin()+count,[&](unsigned a,unsigned b){return dot(smoke_[a].position-eye,view)>dot(smoke_[b].position-eye,view);});
        first=unsigned(mesh.vertices.size());
        for(unsigned i=0;i<count;++i){const auto& p=smoke_[order[i]];
            const float tint=night?.38f:.82f,fade=std::clamp(p.life/.65f,0.f,1.f);
            const Color color{tint,tint,tint,.5f*p.strength*fade};
            const auto r=across*p.size,u=up*p.size;
            const Vertex a{p.position-r+u,-view,color,0,0},b{p.position+r+u,-view,color,1,0},c{p.position+r-u,-view,color,1,1},d{p.position-r-u,-view,color,0,1};
            triangle(a,b,c);triangle(a,c,d);
        }
        if(mesh.vertices.size()>first)mesh.ranges.push_back({first,unsigned(mesh.vertices.size())-first,smokeTexture,tsp|(3u<<6)|(1u<<13)|(1u<<15)|(1u<<16),pcw|8u,isp,512,true});
    }
    unsigned markCount()const{unsigned n=0;for(const auto& m:marks_)n+=m.life>0;return n;}
    unsigned smokeCount()const{unsigned n=0;for(const auto& p:smoke_)n+=p.life>0;return n;}
private:
    struct Mark{std::array<Vec3,4> points{};float life=0,strength=0;};
    struct Smoke{Vec3 position{},velocity{};float life=0,size=0,strength=0;unsigned owner=0;};
    std::array<Mark,markCapacity> marks_{};
    std::array<Smoke,smokeCapacity> smoke_{};
    unsigned markCursor_=0,smokeCursor_=0;
    std::array<std::array<Vec3,2>,2> lastPoints_{},lastNormals_{};
    std::array<Vec3,2> lastCar_{};
    std::array<float,2> emission_{};
    std::array<bool,2> connected_{},seen_{};
};
}
