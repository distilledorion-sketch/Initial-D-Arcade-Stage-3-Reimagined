#include "original_tuning_preview.h"
#include "original_tuning_ui.h"
#include <iostream>
using namespace idas3;
using namespace idas3::original;
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("Project root and output required");
    std::filesystem::create_directories(argv[2]);const auto data=OriginalTuningData::load(argv[1]);
    const auto ui=OriginalTuningUi::load(argv[1]);
    for(unsigned car:{0u,25u,30u}){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(72,1000000);p.setu(76,5);p.setByte(154,0);
        OriginalTuningPreviewPresentation preview;preview.load(argv[1],p);
        Renderer renderer;if(!renderer.initialize(nullptr,640,480,true))throw std::runtime_error(renderer.error);
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};renderer.fitOriginalViewport=true;
        renderer.verticalFieldOfView=preview.verticalFieldOfView;renderer.projectionAspect=preview.aspect;
        renderer.nearClip=preview.nearClip;renderer.farClip=preview.farClip;
        if(!preview.upload(renderer))throw std::runtime_error(renderer.error);
        for(unsigned tick=0;tick<=120;++tick){
            preview.consume({},p,data,OriginalTuningChildKind::none);
            if(tick!=0&&tick!=60&&tick!=120)continue;
            if(!renderer.draw(preview.mesh(),preview.eye,preview.target,false,false,nullptr,false,&preview.lighting))throw std::runtime_error(renderer.error);
            const auto output=std::filesystem::path(argv[2])/("car"+std::to_string(car)+"-tick"+std::to_string(tick)+".bmp");
            if(!renderer.saveBitmap(output.wstring()))throw std::runtime_error(renderer.error);
            std::cout<<output.string()<<'\n';
        }
    }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
