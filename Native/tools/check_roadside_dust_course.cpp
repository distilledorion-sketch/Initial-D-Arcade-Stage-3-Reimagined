#include "original_collision.h"
#include "original_roadside_dust.h"
#include "weather_shelter.h"
#include "course_scene_catalog.h"
#include "course.h"
#include <fstream>
#include <iostream>
#include <map>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Native root and NEW output CSV required");
    const std::filesystem::path root=argv[1],output=argv[2];
    if(std::filesystem::exists(output))throw std::runtime_error("Preserve previous evidence");
    std::ofstream csv(output);csv<<"variant,index,lane,x,y,z,flags,covered,dust\n";
    const auto course=Course::load(root/"data/courses","k_tu");
    for(unsigned variant=0;variant<2;++variant){
        const auto data=original::OriginalCollisionData::load(root/"data/original_physics"/("collision_k_tu_"+std::to_string(variant)+".rcl"));
        const auto scene=OriginalCourseScene::load(root,"k_tu",false,variant!=0,false);
        WeatherShelter roof;roof.build(scene.model,scene.assemblies());
        original::OriginalCollisionQuery q;original::OriginalTriangleSearchTrace trace;original::OriginalSurfaceScratch scratch;
        std::map<unsigned,unsigned> inside,outside;
        unsigned hits=0,covered=0,dust=0,coveredDust=0,tunnelHits=0,tunnelDust=0,approachDust=0,exitDust=0;
        for(unsigned index=0;index<course.points.size();++index){
            const auto center=course.points[index];
            std::array<Vec3,21> lanes;lanes[0]=center;
            for(unsigned i=1;i<=10;++i){const float t=float(i)*.1f;
                lanes[i]=center*(1-t)+course.left[index]*t;lanes[10+i]=center*(1-t)+course.right[index]*t;}
            for(unsigned lane=0;lane<lanes.size();++lane){
                const auto pos=lanes[lane]+Vec3{0,1,0};
                original::clearOriginalCollisionQuery(q);q.setf(32,pos.x);q.setf(36,pos.y);q.setf(40,pos.z);
                if(!original::queryOriginalCollisionSurface(data,q,trace,scratch)||q.f(4)<.2f)continue;
                const Vec3 hit{q.f(12),q.f(16),q.f(20)};const bool sheltered=roof.covered(hit+Vec3{0,1,0});
                original::OriginalRoadsideDustInput input;input.course=7;input.wheelSurfaces.fill(std::uint8_t(q.u(28)));
                const bool emits=original::originalRoadsideDustDecision(input).replacesTireSmoke;
                ++hits;covered+=sheltered;dust+=emits;coveredDust+=sheltered&&emits;
                // The roof classifier also sees trees/bridges and sloping
                // roadside geometry; coveredDust is diagnostic, not a tunnel
                // assertion. Use the known tunnel interior and both portals.
                if(index>=2420&&index<=2550){++tunnelHits;tunnelDust+=emits;}
                if(index==2380)approachDust+=emits;
                if(index==2600)exitDust+=emits;
                (sheltered?inside:outside)[q.u(28)]++;
                csv<<variant<<','<<index<<','<<lane<<','<<hit.x<<','<<hit.y<<','<<hit.z<<','<<q.u(28)<<','<<sheltered<<','<<emits<<'\n';
            }
        }
        std::cout<<"variant="<<variant<<" hits="<<hits<<" covered="<<covered<<" dust="<<dust<<" coveredDust="<<coveredDust<<'\n';
        for(const auto& [flags,count]:inside)std::cout<<" covered flags="<<std::hex<<flags<<std::dec<<" count="<<count<<'\n';
        for(const auto& [flags,count]:outside)std::cout<<" outside flags="<<std::hex<<flags<<std::dec<<" count="<<count<<'\n';
        if(tunnelHits!=131*21||tunnelDust||approachDust!=4||exitDust!=4)
            throw std::runtime_error("Tsuchisaka tunnel/shoulder contact contract changed");
        std::cout<<" PASS "<<tunnelHits<<" interior lane probes without dust; four dust lanes at each exterior portal approach\n";
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
