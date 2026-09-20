#include "car_shadow.h"
#include "renderer.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace idas3;
int main()try{
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    // A sloping, banked road plane under a rotated car footprint.
    const auto road=[](float x,float z){return Vec3{x,100+.12f*x-.07f*z,z};};
    CarShadowFootprint footprint;footprint.roadPoints={road(-.8f,1.3f),road(.8f,1.3f),road(-.8f,-1.2f),road(.8f,-1.2f)};
    Mesh mesh;mesh.vertices.push_back({});mesh.ranges.push_back({0,1});
    require(appendCarContactShadow(mesh,footprint),"Valid shadow missing");
    require(mesh.ranges.size()==2&&mesh.ranges.back().first==1,"Existing mesh was altered");
    const auto& range=mesh.ranges.back();
    require(range.count==864&&range.count+range.first==mesh.vertices.size(),"Invalid shadow range");
    require(range.original&&((range.pcw>>24)&7)==2&&((range.tsp>>29)&7)==4&&((range.tsp>>26)&7)==5,"Shadow must use alpha blending");
    require(((range.isp>>29)&7)==6&&(range.isp&(1u<<26))&&(range.tsp&(1u<<20))&&(range.gmp&512),"Shadow must depth-test without writing depth, preserving unlit alpha");
    const Vec3 normal=normalized(Vec3{-.12f,1,.07f});std::size_t transparent=0;
    for(std::size_t i=1;i<mesh.vertices.size();++i){const auto& v=mesh.vertices[i];
        const float distance=dot(v.position-Vec3{0,100,0},normal);
        require(std::abs(distance-footprint.surfaceLift)<.00002f,"Shadow floated away from the road plane");
        require(v.color.a>=0&&v.color.a<=footprint.opacity,"Shadow alpha outside range");
        require(dot(v.normal,normal)>.99999f,"Shadow normal does not follow the road");
        if(v.color.a==0)++transparent;
    }
    require(transparent==96,"Outer shadow boundary must be fully transparent");
    for(std::size_t i=1;i<mesh.vertices.size();i+=3)
        require(dot(cross(mesh.vertices[i+1].position-mesh.vertices[i].position,mesh.vertices[i+2].position-mesh.vertices[i].position),normal)>0,"Shadow triangle winding reversed");
    const auto size=mesh.vertices.size();footprint.separation=2;
    require(!appendCarContactShadow(mesh,footprint)&&mesh.vertices.size()==size,"Airborne shadow did not fade away");
    footprint.separation=0;footprint.roadPoints[0].x=std::numeric_limits<float>::quiet_NaN();
    require(!appendCarContactShadow(mesh,footprint)&&mesh.vertices.size()==size,"Invalid contact corrupted mesh");
    std::cout<<"PASS soft car shadow slope/bank conformity, transparent boundary, material state, winding and airborne fade. No graphics device created.\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
