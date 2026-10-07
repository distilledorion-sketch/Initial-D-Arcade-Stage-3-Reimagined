#include "headlight_glare.h"
#include "car_presentation.h"
#include "car_catalog.h"
#include <iostream>
using namespace idas3;
int main(int argc,char** argv)try{
    if(argc!=3)throw std::runtime_error("Arguments: Native root, evidence folder");
    const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
    unsigned checks=0;const auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
    HeadlightGlare glare;glare.load(argv[1]);
    check(HeadlightGlare::strength({},{0,0,1},{0,0,10})==1,"Head-on glare missing");
    check(HeadlightGlare::strength({},{0,0,1},{0,0,-10})==0,"Glare visible from behind");
    check(HeadlightGlare::strength({},{0,0,1},{101,0,10})==0,"Glare exceeds source100m range");
    const float diagonal=HeadlightGlare::strength({},{0,0,1},{10,0,10});
    check(diagonal>.1f&&diagonal<.13f,"Glare lost source sixth-power facing");
    const Vec3 eye{0,.6f,0},aim{0,.6f,-1},body{0,.3f,-12};
    for(unsigned car=0;car<35;++car){
        Mesh mesh;const auto count=glare.append(mesh,car,body,0,0,0,true,1,eye,aim,{0,1,0},0,1);
        check(count==(car==20?4:2),"Missing source lamp anchors");
        for(const auto& range:mesh.ranges)check(range.viewMask==1&&range.emissive&&(range.isp&(1u<<26)),"Glare writes depth or lost view scope");
        Mesh dark;check(!glare.append(dark,car,body,0,0,0,false,1,eye,aim,{0,1,0},0,3),"Disabled lamps still glow");
        check(!glare.append(dark,car,body,0,0,0,true,0,eye,aim,{0,1,0},0,3),"Closed popup lamps still glow");
    }
    constexpr unsigned w=640,h=480;Renderer renderer;
    check(renderer.initialize(nullptr,w,h,true),renderer.error.c_str());
    renderer.overrideClearColor=true;renderer.clearColor={.02f,.025f,.03f,1};
    check(renderer.loadTextures(glare.textures),renderer.error.c_str());
    OriginalRearViewFrame rear;rear.eye=eye;rear.target=aim;rear.up={0,1,0};
    const auto capture=[&](const Mesh& mesh,const char* name){
        check(renderer.draw(mesh,eye,aim,true,false,nullptr,false,nullptr,&rear),renderer.error.c_str());
        const auto path=out/(std::string(name)+".bmp");check(renderer.saveBitmap(path.wstring()),renderer.error.c_str());
        std::ifstream in(path,std::ios::binary);BITMAPFILEHEADER head{};in.read(reinterpret_cast<char*>(&head),sizeof head);in.seekg(head.bfOffBits);
        std::vector<unsigned> pixels(w*h);in.read(reinterpret_cast<char*>(pixels.data()),pixels.size()*4);check(bool(in),"Glare readback failed");return pixels;
    };
    const auto dark=capture({},"glare-off");
    const auto differences=[&](const auto& pixels,bool mirror){unsigned count=0;
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){const bool inside=x>=160&&x<480&&y>=32&&y<96;
            if(inside==mirror&&pixels[y*w+x]!=dark[y*w+x])++count;}
        return count;
    };
    for(unsigned mask:{1u,2u,3u}){
        Mesh mesh;glare.append(mesh,0,body,0,0,0,true,1,eye,aim,{0,1,0},0,mask);
        const auto pixels=capture(mesh,("glare-mask-"+std::to_string(mask)).c_str());
        check((differences(pixels,false)>100)==bool(mask&1),"Main-view glare missing or leaked camera mask");
        check((differences(pixels,true)>100)==bool(mask&2),"Rearview glare missing or leaked camera mask");
        check(capture(mesh,"glare-repeat")==pixels,"Glare changed on identical repaint");
    }
    Mesh wall;wall.box({0,.6f,-5},{20,10,1},0,{.05f,.05f,.05f,1});
    const auto blocked=capture(wall,"glare-wall");
    glare.append(wall,0,body,0,0,0,true,1,eye,aim,{0,1,0},0,3);
    check(capture(wall,"glare-occluded")==blocked,"Glare shines through scenery");
    // Use the actual assembled car bodies as occluders too. Lamp positions
    // must sit at the lenses, rather than being hidden inside the bonnet.
    for(unsigned car:{0u,8u,19u,20u}){
        const std::filesystem::path root=argv[1];const std::string folder(originalCarFolders.at(car));
        auto model=NativeModel::load(root/"data/original_models"/folder/(folder+".idasmesh"));
        auto presentation=CarPresentation::load(root,car,model.chunks.size());presentation.applyMaterials(model);
        auto textures=NativeTextureBank::load(root/"data/original_assets/cars"/folder/"textures/textures.idastex");
        check(renderer.loadTextures(textures),renderer.error.c_str());
        check(renderer.loadTextures(glare.textures,true),renderer.error.c_str());
        presentation.advanceOriginalFrame(true);Mesh mesh;
        mesh.originalCar(model,presentation.pose({},true),body,0,0,0,0,presentation.illuminatedChunks());
        const auto before=capture(mesh,("car-"+std::to_string(car)+"-before").c_str());
        glare.append(mesh,car,body,0,0,0,true,1,eye,aim,{0,1,0},unsigned(textures.size()),3);
        const auto after=capture(mesh,("car-"+std::to_string(car)+"-after").c_str());
        unsigned changed=0;for(std::size_t i=0;i<before.size();++i)changed+=before[i]!=after[i];
        check(changed>100,"Glare anchors hidden by actual car body");
    }
    std::cout<<"PASS "<<checks<<" original lamp, facing, toggle, popup, camera mask, GPU visibility and occlusion checks.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
