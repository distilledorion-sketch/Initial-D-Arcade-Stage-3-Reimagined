#include "original_ending.h"
#include <stdexcept>

namespace idas3::original {
void OriginalEnding::begin(const std::filesystem::path& root){
    if(model_.chunks.empty()){
        const auto folder=root/"data/original_assets/ending";
        auto model=NativeModel::load(folder/"ending.idasmesh");
        auto textures=NativeTextureBank::load(folder/"textures/textures.idastex");
        if(model.chunks.size()!=6||textures.size()!=17)
            throw std::runtime_error("Incomplete ending artwork");
        model_=std::move(model);textures_=std::move(textures);
    }
    timeline={};
}
void OriginalEnding::paint(std::span<std::uint32_t> pixels,int width,int height)const{
    const float fit=std::min(float(width)/640.f,float(height)/480.f);
    SpritePlacement placement;placement.invertY=true;placement.authoredHeight=0;
    placement.scale=100.f*fit;placement.offsetX=(width-640.f*fit)*.5f;
    placement.offsetY=(height-480.f*fit)*.5f;
    const auto draw=[&](unsigned chunk,float y=0){
        auto translated=placement;translated.offsetY-=y*100.f*fit;
        compositeOriginalMenuChunk(pixels,width,height,textures_,model_.chunks.at(chunk),translated);
    };
    if(timeline.finalCard){draw(0);draw(1);draw(2);}
    else {draw(3,timeline.creditsY);draw(4,timeline.photosY);}
}
}
