#include "original_course_objects.h"
#include "course_scene_catalog.h"
#include "course.h"
#include <chrono>
#include <iostream>
#include <map>
#include <tuple>
using namespace idas3;
using Key=std::tuple<std::size_t,std::uint32_t,std::array<float,16>>;
std::map<Key,unsigned> census(const std::vector<NativeAssemblyInsertion>& groups){
    std::map<Key,unsigned> result;
    for(const auto& group:groups)for(const auto& instance:group.assembly.instances)
        ++result[{group.before,instance.chunk,instance.transform}];
    return result;
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Native root required");const std::filesystem::path root=argv[1];
    unsigned variants=0,cases=0,expanded=0,maxTrees=0;double selectionMs=0;
    for(const std::string id:{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"})
    for(bool night:{false,true})for(bool reverse:{false,true})for(bool wet:{false,true}){
        auto scene=OriginalCourseScene::load(root,id,night,reverse,wet);
        const auto route=Course::load(root/"data/courses",id=="k_df3"?"k_df":id);
        auto objects=OriginalCourseObjects::load(root,id,night,reverse,wet,scene.model.chunks.size());
        for(std::size_t i=0;i<route.points.size();i+=97){
            const auto count=scene.assemblyForPathIndex(i).instances.size();
            objects.setMinimumDrawDistance(0);
            const auto before=census(objects.insertionsForPathIndex(i,route.points[i],count));
            objects.setMinimumDrawDistance(600);
            const auto start=std::chrono::steady_clock::now();
            const auto groups=objects.insertionsForPathIndex(i,route.points[i],count);
            selectionMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            const auto after=census(groups);unsigned draws=0;
            for(const auto& [key,n]:before){const auto it=after.find(key);
                if(it==after.end()||it->second<n)throw std::runtime_error(id+" extended distance removed an existing tree");}
            for(const auto& [key,n]:after){
                const auto& m=std::get<2>(key);const auto p=route.points[i];
                const double x=m[3]-p.x,y=m[7]-p.y,z=m[11]-p.z;
                if(!before.contains(key)&&x*x+y*y+z*z>600.01*600.01)throw std::runtime_error("New tree exceeds distance bound");
                if(std::get<1>(key)>=scene.model.chunks.size())throw std::runtime_error("Tree chunk outside bank");draws+=n;
            }
            if(after.size()>before.size())++expanded;
            maxTrees=std::max(maxTrees,draws);++cases;
            objects.setMinimumDrawDistance(0);
            if(before!=census(objects.insertionsForPathIndex(i,route.points[i],count)))throw std::runtime_error("Distance toggle changed authored/source state");
        }
        ++variants;
    }
    if(variants!=72||expanded<100)throw std::runtime_error("Insufficient extended-distance coverage");
    std::cout<<"PASS "<<variants<<" variants, "<<cases<<" path cases, "<<expanded<<" expanded selections; existing trees retained, new trees within600m, source state restored; max draws="<<maxTrees<<", mean selection ms="<<selectionMs/cases<<'\n';
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
