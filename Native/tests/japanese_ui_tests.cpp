#include "native_assets.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace fs=std::filesystem;
using namespace idas3;
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void bitmap(const fs::path& path,int w,int h,const std::vector<std::uint32_t>& pixels){
    std::ofstream out(path,std::ios::binary);
    auto u16=[&](unsigned n){out.put(char(n));out.put(char(n>>8));};
    auto u32=[&](std::uint32_t n){u16(n&65535);u16(n>>16);};
    u16(0x4d42);u32(54+w*h*4);u32(0);u32(54);u32(40);u32(w);u32(std::uint32_t(-h));
    u16(1);u16(32);u32(0);u32(w*h*4);u32(0);u32(0);u32(0);u32(0);
    out.write(reinterpret_cast<const char*>(pixels.data()),std::streamsize(pixels.size()*4));
}
int main(int argc,char** argv)try{
    require(argc==3,"Usage: japanese_ui_tests nativeRoot outputDirectory");
    const fs::path root=argv[1],output=argv[2],base=root/"data/original_assets",jp=root/"data/localization/ja/original_assets";
    fs::create_directories(output);unsigned banks=0;
    for(const auto& entry:fs::recursive_directory_iterator(jp)){
        if(entry.path().extension()!=".idasmesh")continue;
        const auto relative=fs::relative(entry.path(),jp),modelPath=base/relative;
        const auto texturePath=modelPath.parent_path()/"textures/textures.idastex";
        setOriginalUiLanguage(0);
        require(localizedUiAssetPath(modelPath)==modelPath,"English must use the original asset");
        const auto english=NativeModel::load(modelPath);const auto enTextures=NativeTextureBank::load(texturePath);
        setOriginalUiLanguage(1);
        require(fs::equivalent(localizedUiAssetPath(modelPath),entry.path()),"Japanese resolver did not select mapped bank");
        const auto japanese=NativeModel::load(modelPath);const auto jaTextures=NativeTextureBank::load(texturePath);
        require(japanese.chunks.size()==english.chunks.size(),"Localization changed runtime chunk IDs");
        require(jaTextures.sourceSize()==enTextures.size()&&jaTextures.size()>=enTextures.size(),"Localization changed source texture IDs");
        for(unsigned i=0;i<enTextures.size();++i){const auto& a=enTextures.at(i);const auto& b=jaTextures.at(i);
            require(a.width==b.width&&a.height==b.height&&a.argb==b.argb,"English fallback pixels changed");}
        for(unsigned i=0;i<japanese.chunks.size();++i){
            require(japanese.chunks[i].index==i,"Localized chunk ID mismatch");
            for(const auto& b:japanese.chunks[i].batches)
                require(b.material[9]==0xffffffffu||b.material[9]<jaTextures.size(),"Localized texture reference out of bounds");
        }
        ++banks;
    }
    require(banks>=40,"Incomplete Japanese UI package");
    const auto car=root/"data/original_assets/cars/model.idasmesh";
    require(localizedUiAssetPath(car)==car,"Car path must not be localized");
    const auto missing=root/"data/original_assets/menus/not_present.idasmesh";
    require(localizedUiAssetPath(missing)==missing,"Missing localized art must retain English");
    bool rejected=false;try{setOriginalUiLanguage(2);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected&&originalUiLanguage()==1,"Invalid language changed active selection");
    struct Sample{const char* path;unsigned chunk;};
    const Sample samples[]={
        {"menus/v3/v3sS11mode/v3sS11mode.idasmesh",0},
        {"menus/v3/v3sS05cars/v3sS05cars.idasmesh",43},
        {"menus/v3/v3sK00common/v3sK00common.idasmesh",31},
        {"menus/v3/v3sT02route/v3sT02route.idasmesh",0},
        {"hud/game2d/game2d.idasmesh",9},
        {"hud/game2d/game2d.idasmesh",147},
        {"menus/v3/v3sA00etc/v3sA00etc.idasmesh",6},
        {"results/v3sT13rankin/v3sT13rankin.idasmesh",46},
        {"rival_dialog/banks/rival_scene/rival_scene.idasmesh",19},
        {"menus/warnings/select0402/select0402.idasmesh",0}};
    constexpr int width=1100,rowHeight=100,height=1000;
    std::vector<std::uint32_t> pixels(width*height,0xff20242b);
    unsigned row=0;
    for(const auto& sample:samples){for(int language=0;language<2;++language){
        setOriginalUiLanguage(language);const auto path=base/sample.path;
        const auto model=NativeModel::load(path);const auto textures=NativeTextureBank::load(path.parent_path()/"textures/textures.idastex");
        const auto& chunk=model.chunks.at(sample.chunk);
        float left=1e9f,right=-1e9f,top=1e9f,bottom=-1e9f;
        for(const auto& b:chunk.batches)for(const auto& v:b.vertices){left=std::min(left,v.position.x);right=std::max(right,v.position.x);top=std::min(top,-v.position.y);bottom=std::max(bottom,-v.position.y);}
        SpritePlacement placement;placement.invertY=true;placement.authoredHeight=0;placement.defaultOriginalUiColors=true;placement.softwareOnly=true;
        placement.scale=std::min(500.f/std::max(.01f,right-left),75.f/std::max(.01f,bottom-top));
        placement.offsetX=language*550+25-left*placement.scale;placement.offsetY=row*rowHeight+12-top*placement.scale;
        compositeOriginalMenuChunk(pixels,width,height,textures,chunk,placement);
    }++row;}
    bitmap(output/"english-japanese-ui.bmp",width,height,pixels);
    setOriginalUiLanguage(0);
    std::cout<<"PASS: "<<banks<<" localized banks; stable IDs, all fallback pixels, bounds, language selection and native render\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
