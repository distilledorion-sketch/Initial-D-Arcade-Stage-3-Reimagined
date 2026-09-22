#include "driving_effects.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
int main()try{
    const auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    DrivingEffects effects;std::array<DrivingEffects::Car,2> cars{};
    auto& car=cars[0];car.visible=car.grounded=true;car.speed=20;car.slip=-.4f;
    const auto move=[&](float z){car.position={0,0,z};for(unsigned i=0;i<4;++i){const float x=(i&1)?.7f:-.7f;const float dz=i<2?1.2f:-1.2f;
        car.points[i]={x,.1f*x+.05f*(z+dz),z+dz};car.normals[i]=normalized(Vec3{-.1f,1,-.05f});}};
    for(unsigned i=0;i<60;++i){move(i*.3f);effects.advance(1./60,true,false,cars);}
    check(effects.markCount()>50&&effects.smokeCount()>0,"Signed slip produces both effects");
    Mesh mesh;effects.append(mesh,{0,5,10},{0,0,20},123,false);
    check(mesh.ranges.size()==2&&mesh.ranges.back().texture==123,"Smoke and rubber are independent batched materials");
    for(const auto& range:mesh.ranges)check(((range.pcw>>24)&7)==2&&(range.isp&(1u<<26)),"Effects use translucent no-depth-write state");
    const auto normal=normalized(Vec3{-.1f,1,-.05f});
    for(unsigned i=0;i<mesh.ranges[0].count;++i)check(std::abs(dot(mesh.vertices[i].position,normal)-.012f)<.00001f,"Rubber must stay on the banked/sloped road");
    const auto marks=effects.markCount(),smoke=effects.smokeCount();
    move(25);effects.advance(.1,true,true,cars);check(effects.markCount()==marks&&effects.smokeCount()==smoke,"Pause freezes effects");
    car.grounded=false;effects.advance(.1,true,false,cars);check(effects.markCount()==marks,"Airborne car cannot leave marks");
    car.grounded=true;move(100);effects.advance(1./60,true,false,cars);check(effects.markCount()==marks,"No ribbon across teleport");
    car.slip=0;for(unsigned i=0;i<250;++i)effects.advance(.1,true,false,cars);
    check(effects.markCount()==0&&effects.smokeCount()==0,"Effects expire without retaining stale geometry");
    car.slip=.5f;
    for(unsigned i=0;i<10000;++i){move(100+i*.3f);effects.advance(1./60,true,false,cars);}
    check(effects.markCount()<=DrivingEffects::markCapacity&&effects.smokeCount()<=DrivingEffects::smokeCapacity,"Long race memory is bounded");
    effects.advance(1./60,false,false,cars);check(!effects.markCount()&&!effects.smokeCount(),"Wet weather/menu reset dry effects");
    std::cout<<"PASS dry tire effects: signed slip, bank/slope contact, smoke texture, material state, pause, airborne gaps, teleports, expiry and capacity.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
