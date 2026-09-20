#include "akina_background.h"
#include <cmath>
#include <iostream>
#include <stdexcept>
static void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char**argv){try{
    using namespace idas3;
    require(argc==2,"Project root required");std::filesystem::path root=argv[1];
    const auto model=NativeModel::load(root/originalAkinaBackgroundModel);
    const auto bank=NativeTextureBank::load(root/originalAkinaBackgroundTextures);
    require(model.chunks.size()==1&&bank.size()==2,"Original background bank count mismatch");
    const auto& chunk=model.chunks.front();require(chunk.batches.size()==3,"Original background batch count mismatch");
    std::size_t vertices=0,triangles=0;
    for(const auto& batch:chunk.batches){
        require(batch.material[2]&512,"Background must retain its original constant-color flag");
        require(batch.material[9]<bank.size(),"Background texture binding exceeds bank");
        vertices+=batch.vertices.size();triangles+=batch.indices.size()/3;
        for(auto index:batch.indices)require(index<batch.vertices.size(),"Background triangle index exceeds batch");
    }
    require(vertices==334&&triangles==264,"Authored background geometry changed");
    require(bank.at(0).width==512&&bank.at(0).height==256&&bank.at(1).width==128&&bank.at(1).height==128,"Original texture dimensions changed");
    for(Vec3 camera: {Vec3{0,1000,0},Vec3{420,671,-2100},Vec3{-1500,1200,3300}}){
        const auto instance=originalAkinaBackgroundInstance(camera);const auto& m=instance.transform;
        require(instance.chunk==0&&m[3]==camera.x&&m[7]==0&&m[11]==camera.z,"Horizontal camera anchoring changed");
        require(m[0]==1&&m[5]==1&&m[10]==1&&m[15]==1,"Background must not be scaled");
        for(const auto& batch:chunk.batches)for(const auto& v:batch.vertices){
            const Vec3 point{v.position.x+m[3],v.position.y+m[7],v.position.z+m[11]};
            require(point.y==v.position.y,"Authored background height moved with camera");
            require(std::abs((point.x-camera.x)-v.position.x)<.001f&&std::abs((point.z-camera.z)-v.position.z)<.001f,"Horizontal background parallax introduced");
        }
    }
    std::cout<<"PASS original Akina background: native model/texture parser,334 vertices/264 triangles,3 material batches,2 textures,horizontal camera anchor with authored Y and scale preserved\n";
    return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
