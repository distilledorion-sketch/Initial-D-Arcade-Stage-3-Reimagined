#include "native_assets.h"
#include "car_catalog.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace idas3;
namespace fs=std::filesystem;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Pass game root");const fs::path root=argv[1];
    NativeImage image{2,2,{0xffff0000,0xff00ff00,0xff0000ff,0xffffffff}};
    std::vector<std::uint32_t> canvas(64);
    compositeImage(canvas,8,8,image,0,0,8,8,.5f);
    for(int y=0;y<8;y++)for(int x=0;x<8;x++){
        auto pixel=canvas[y*8+x];check((pixel>>24)==128,"Sprite shared diagonal left a hole or blended twice");
        check((pixel&0xffffff)==(image.argb[(y/4)*2+x/4]&0xffffff),"Sprite UV orientation or texel coordinate changed");
    }
    std::fill(canvas.begin(),canvas.end(),0xff112233);compositeImage(canvas,8,8,image,-4,-4,8,8,0);
    for(auto p:canvas)check(p==0xff112233,"Zero-alpha image changed destination");
    auto model=NativeModel::load(root/"data/original_models/toyota_ae86t/toyota_ae86t.idasmesh");
    std::size_t vertices=0,triangles=0,batches=0;
    for(const auto& c:model.chunks)for(const auto& b:c.batches){vertices+=b.vertices.size();triangles+=b.indices.size()/3;batches++;}
    check(model.chunks.size()==106&&vertices==39566&&triangles==29476&&batches==349,"Original AE86 model counts changed");
    std::size_t carModels=0,carTextures=0,drawInstances=0;
    for(const auto folderView:originalCarFolders){
        const std::string folder(folderView);const auto directory=root/"data/original_models"/folder;
        auto car=NativeModel::load(directory/(folder+".idasmesh"));auto assembly=NativeAssembly::load(directory/"assembly"/(folder+"_default.idasasm"),car.chunks.size());
        auto bank=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        for(const auto& instance:assembly.instances){check(car.chunks.at(instance.chunk).header[0]!=0xffffffff,"Original selected a non-geometry marker");for(const auto& batch:car.chunks[instance.chunk].batches)check(batch.material[9]==0xffffffff||batch.material[9]<bank.size(),"Car assembly references missing texture");}
        carModels++;carTextures+=bank.size();drawInstances+=assembly.instances.size();
    }
    check(carModels==35&&carTextures==1916&&drawInstances==659,"All-car import catalog mismatch");
    std::size_t banks=0,sprites=0,textures=0;
    for(const auto& entry:fs::directory_iterator(root/"data/original_assets/menus")){
        const auto p=entry.path();if(!fs::exists(p/"original_rip.tbl"))continue;
        auto tex=NativeTextureBank::load(p/"textures/textures.idastex");auto bank=NativeSpriteBank::load(p/"original_rip.tbl",p/"original_rip.bin");
        for(const auto& sprite:bank.sprites)check(sprite.texture<tex.size(),"Sprite references missing texture");
        sprites+=bank.sprites.size();textures+=tex.size();banks++;
    }
    check(banks==21&&sprites==211&&textures==170,"Original RIP menu catalog mismatch");
    std::cout<<"PASS native assets: "<<banks<<" RIP banks, "<<sprites<<" sprites, "<<textures<<" menu textures; "<<carModels<<" original cars/"<<carTextures<<" car textures/"<<drawInstances<<" selected instances; exact source validation and alpha/UV clipping checks\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
