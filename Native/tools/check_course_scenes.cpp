#include "course_scene_catalog.h"
#include <iostream>
#include <stdexcept>
int main(int argc,char**argv){try{
    if(argc<2)throw std::runtime_error("Root required");const std::filesystem::path root(argv[1]);unsigned variants=0,draws=0;
    for(const std::string id:{"k_ez","s_nm","h_hd","k_df","s_vh","s_uh","n_sy","k_tu","k_df3"})for(bool night:{false,true})for(bool reverse:{false,true})for(bool wet:{false,true}){
        const bool expected=true;
        if(idas3::OriginalCourseScene::available(root,id,night,reverse,wet)!=expected)throw std::runtime_error("Wrong original course availability gate");
        if(!expected)continue;
        auto scene=idas3::OriginalCourseScene::load(root,id,night,reverse,wet);auto model=std::move(scene.model);auto background=std::move(scene.backgroundModel);
        for(std::size_t i=0;i<5000;++i){const auto& assembly=scene.assemblyForPathIndex(i);if(assembly.instances.empty())throw std::runtime_error("Empty original course assembly");for(const auto& instance:assembly.instances){if(instance.chunk>=model.chunks.size())throw std::runtime_error("Original course chunk outside bank");++draws;}}
        const auto sky=scene.backgroundAssembly({17,83,-41});if(sky.instances.empty()||sky.instances[0].transform[3]!=17||sky.instances[0].transform[7]!=0||sky.instances[0].transform[11]!=-41)throw std::runtime_error("Original sky anchoring failed");
        for(const auto& instance:sky.instances)if(instance.chunk>=background.chunks.size())throw std::runtime_error("Original sky chunk bound");
        ++variants;std::cout<<id<<" night"<<night<<" reverse"<<reverse<<" wet"<<wet<<": "<<model.chunks.size()<<" source chunks, "<<scene.assemblyCount()<<" assemblies\n";
    }
    if(idas3::OriginalCourseScene::available(root,"missing",false,false))throw std::runtime_error("Incomplete course claimed available");
    std::cout<<"PASS: "<<variants<<" course variants; "<<draws<<" selected draw bounds checked; moved-bank ownership and original camera-XZ sky placement preserved.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
