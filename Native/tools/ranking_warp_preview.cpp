#include "original_ranking_presentation.h"
#include "car_catalog.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("projectRoot outputDirectory required");
    const std::filesystem::path root=argv[1],out=argv[2];std::filesystem::create_directories(out);
    auto records=OriginalRankingRecords::load(root/"data/original_assets/attract/ranking/factory_records.idasrank");
    OriginalRankingPlayback playback;playback.reset(0);
    OriginalRankingPresentation presentation;presentation.load(root);
    for(const auto [width,height]:{std::pair{640,480},std::pair{1280,720}}){
        Renderer renderer;if(!renderer.initialize(nullptr,width,height,true))throw std::runtime_error(renderer.error);
        renderer.overrideClearColor=true;renderer.clearColor={0,0,0,1};renderer.fitOriginalViewport=true;
        renderer.verticalFieldOfView=presentation.verticalFieldOfView;renderer.projectionAspect=presentation.aspect;
        renderer.nearClip=presentation.nearClip;renderer.farClip=presentation.farClip;
        std::uint64_t revision=~0ull;playback.reset(0);
        for(unsigned tick=1;tick<=330;++tick){
            playback.step(records);
            if(tick!=105&&tick!=150&&tick!=330)continue;
            const auto& mesh=presentation.mesh(playback);
            if(revision!=presentation.textureRevision()){
                if(!renderer.loadTextures(presentation.groundTextures())||!renderer.loadTextures(presentation.carTextures(),true)
                    ||!renderer.loadTextures(presentation.plateTextures(),true)
                    ||!renderer.loadTextures(presentation.environmentTextures(),true))throw std::runtime_error(renderer.error);
                revision=presentation.textureRevision();
            }
            const auto& board=presentation.paint(width,height,playback,records);
            if(tick==150){
                // Same native frame/renderer, with only the formerly omitted
                // image replacement reverted, provides a bounded visual A/B.
                auto before=mesh;const auto folder=originalCarFolders.at(playback.resources.car);
                const auto materials=OriginalCarMaterialRebuild::load(root/"data/original_models"/folder/"material_layout.bin",playback.resources.car);
                const auto chunk=materials.semanticChunks().at(140);
                if(chunk<0)throw std::runtime_error("Ranking A/B environment semantic");
                const unsigned priorTexture=unsigned(presentation.groundTextures().size())+materials.chunks().at(unsigned(chunk)).materials.back().words[9];
                const unsigned environment=unsigned(presentation.groundTextures().size()+presentation.carTextures().size()+presentation.plateTextures().size());
                unsigned changed=0;for(auto& range:before.ranges)if(range.texture==environment){range.texture=priorTexture;++changed;}
                if(!changed)throw std::runtime_error("Ranking A/B contains no environment ranges");
                if(!renderer.draw(before,presentation.eye,presentation.target,false,false,board.data(),false,&presentation.lighting))throw std::runtime_error(renderer.error);
                const auto name=std::to_string(width)+"-"+std::to_string(height)+"-tick150-prior-binding.bmp";
                if(!renderer.saveBitmap((out/name).wstring()))throw std::runtime_error(renderer.error);
            }
            if(!renderer.draw(mesh,presentation.eye,presentation.target,false,false,board.data(),false,&presentation.lighting))throw std::runtime_error(renderer.error);
            const auto name=std::to_string(width)+"-"+std::to_string(height)+"-tick"+std::to_string(tick)+".bmp";
            if(!renderer.saveBitmap((out/name).wstring()))throw std::runtime_error(renderer.error);
            std::cout<<name<<" car="<<playback.resources.car<<" slide="<<playback.events.slideX<<" yaw="<<playback.events.drawYawUnits<<" vertices="<<mesh.vertices.size()<<'\n';
        }
    }
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
