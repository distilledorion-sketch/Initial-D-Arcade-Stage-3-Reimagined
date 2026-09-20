#include "native_assets.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace idas3;
namespace fs=std::filesystem;
void bitmap(const fs::path& path,int w,int h,const std::vector<std::uint32_t>& pixels){
    std::ofstream out(path,std::ios::binary);auto u16=[&](unsigned n){out.put(char(n));out.put(char(n>>8));};auto u32=[&](std::uint32_t n){u16(n&65535);u16(n>>16);};
    u16(0x4d42);u32(54+w*h*4);u32(0);u32(54);u32(40);u32(w);u32(std::uint32_t(-h));u16(1);u16(32);u32(0);u32(w*h*4);u32(0);u32(0);u32(0);u32(0);
    out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));if(!out)throw std::runtime_error("Cannot save menu preview");
}
int main(int argc,char** argv){try{
    if(argc<5)throw std::runtime_error("Usage: preview_original_menu model.idasmesh textures.idastex output.bmp [--ui-colors] [--depth-sort] chunk-index...");
    auto model=NativeModel::load(fs::path(argv[1]));auto textures=NativeTextureBank::load(fs::path(argv[2]));
    constexpr int width=1280,height=960;std::vector<std::uint32_t> pixels(width*height,0xff151719);
    SpritePlacement placement;placement.scale=200;placement.invertY=true;placement.authoredHeight=0;
    // The source UI helper supplies white diffuse values for 0x4a vertices.
    // Raw-geometry inspection remains the default; opt in for UI-owned banks.
    bool depthSort=false;
    for(int arg=4;arg<argc;arg++){
        if(std::string_view(argv[arg])=="--ui-colors")placement.defaultOriginalUiColors=true;
        if(std::string_view(argv[arg])=="--depth-sort")depthSort=true;
    }
    struct Layer {NativeModelChunk chunk;float depth;};std::vector<Layer> layers;
    for(int arg=4;arg<argc;arg++){
        if(std::string_view(argv[arg]).starts_with("--"))continue;
        const auto& chunk=model.chunks.at(std::stoul(argv[arg]));
        if(!depthSort){compositeOriginalMenuChunk(pixels,width,height,textures,chunk,placement);continue;}
        // Optional planar UI inspection: positive authored Z is nearer, as
        // in the name-entry renderer. This is not a general 3D depth buffer.
        for(const auto& batch:chunk.batches){
            if(batch.vertices.empty())continue;
            float z=0;for(const auto& vertex:batch.vertices)z+=vertex.position.z;
            NativeModelChunk part;part.batches.push_back(batch);
            layers.push_back({std::move(part),z/float(batch.vertices.size())});
        }
    }
    if(depthSort){
        std::stable_sort(layers.begin(),layers.end(),[](const auto& a,const auto& b){return a.depth<b.depth;});
        for(const auto& layer:layers)compositeOriginalMenuChunk(pixels,width,height,textures,layer.chunk,placement);
    }
    bitmap(fs::path(argv[3]),width,height,pixels);std::cout<<"Native CPU menu preview saved: "<<argv[3]<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
