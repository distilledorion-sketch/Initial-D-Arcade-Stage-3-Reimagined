#include "original_tuning_preview.h"
#include "original_car_color_catalog.h"
#include "car_catalog.h"
#include <iostream>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
int main(int argc,char**argv)try{
    if(argc!=2)throw std::runtime_error("Project root required");const auto data=OriginalTuningData::load(argv[1]);
    unsigned checks=0,offers=0;
    auto require=[&](bool result,const char* message){++checks;if(!result)throw std::runtime_error(message);};
    for(unsigned car=0;car<35;++car){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setu(76,5);p.setu(72,1000000);
        for(unsigned bad:{originalCarColorCounts[car],8u,0xffffffffu})for(unsigned view=0;view<3;++view){
            auto invalid=p;invalid.setu(64,bad);const auto saved=invalid.words;
            OriginalTuningPreviewPresentation recovered;recovered.load(argv[1],invalid,view==1,view==2);
            require(((recovered.appearance().word>>25)&7)==0,"Invalid saved paint did not use safe factory color");
            require(!recovered.mesh().vertices.empty(),"Recovered results/ranking/continue car missing");
            require(invalid.words==saved,"Paint recovery rewrote the saved driver");
        }
        const auto folder=std::filesystem::path(argv[1])/"data/original_models"/originalCarFolders[car];
        auto materials=OriginalCarMaterialRebuild::load(folder/"material_layout.bin",car);
        for(unsigned color=0;color<8;++color){
            OriginalCarAppearanceConfig config(car);config.word=0xa0000000u|(color<<25);config.paintDirty=true;
            const auto preserved=config.word&~(7u<<25);materials.rebuild(config,0);
            const auto expected=color<originalCarColorCounts[car]?color:0u,rgb=originalCarPaintRgb[car][expected];
            require(materials.state().rgb==std::array<unsigned,3>{rgb>>16,(rgb>>8)&255,rgb&255},"Palette recovery/valid color changed RGB");
            require(((config.word>>25)&7)==expected&&(config.word&~(7u<<25))==preserved,"Paint recovery altered installed parts");
        }
        OriginalTuningPreviewPresentation preview;preview.load(argv[1],p);
        const auto validate=[&]{const auto& mesh=preview.mesh();require(!mesh.vertices.empty(),"Preview empty");
            const auto bankSize=preview.groundTextures().size()+preview.carTextures().size()+preview.plateTextures().size()+preview.environmentTextures().size();
            unsigned reflections=0,environments=0;
            for(const auto& range:mesh.ranges){require(range.texture==0xffffffffu||range.texture<bankSize,"Preview texture extent");
                reflections+=range.originalLightDirection.has_value();environments+=range.texture+1==bankSize;}
            // The original backdrop and shadow each submit two ranges.
            for(const auto& range:mesh.ranges)if(range.first>=mesh.ranges.at(3).first+mesh.ranges.at(3).count)
                require(range.sourceFaceCulling,"Menu car/plate/reflection missing owner culling");
            require(reflections!=0,"Missing source reflection light");require(environments!=0,"Missing original environment selector3");
            for(const auto& v:mesh.vertices)require(std::isfinite(v.position.x)&&std::isfinite(v.position.y)&&std::isfinite(v.position.z),"Nonfinite preview vertex");
            const auto* address=mesh.vertices.data();const auto phase=preview.nextState().angle;
            require(preview.mesh().vertices.data()==address&&preview.nextState().angle==phase,"Host render advanced preview clock");};
        OriginalTuningPreviewPresentation continuation;continuation.load(argv[1],p,false,true);
        for(unsigned frame=0;frame<512;++frame){
            if(frame%64==0){
                const auto& mesh=continuation.mesh();
                const float tangent=std::tan(continuation.verticalFieldOfView*.5f);
                for(const auto& range:mesh.ranges){
                    if(range.texture<continuation.groundTextures().size()||range.originalLightDirection)continue;
                    for(unsigned i=range.first;i<range.first+range.count;++i){const auto q=mesh.vertices.at(i).position-continuation.eye;
                        require(q.z<-.01f,"Continue car crossed camera");
                        const float x=q.x/(-q.z*tangent*continuation.aspect),y=q.y/(-q.z*tangent);
                        require(std::abs(x)<.96f&&y<.5f&&y>-.96f,"Continue car clipped or overlaps choice panel");
                    }
                }
            }
            continuation.consume({},p,data,OriginalTuningChildKind::none);
        }
        validate();require(preview.visibleState().angle==8192,"Constructor angle");
        preview.consume({},p,data,OriginalTuningChildKind::none);
        require(preview.visibleState().angle==8192&&preview.nextState().angle==8320,"Draw then update ordering");
        for(unsigned offer=0;offer<data.car(car).optional.size();++offer){
            p.setByte(154,std::uint8_t(offer));const auto saved=p.words;const auto word=preview.appearance().word;
            const auto apply=[&](unsigned command){const auto mutation=applyOriginalTuningCommand(p,data,command);
                std::vector<OriginalTuningPreviewEvent> events;
                for(const auto& call:mutation.carCalls)events.push_back({OriginalTuningPreviewEvent::Kind::carCall,call});
                preview.consume(events,p,data,OriginalTuningChildKind::optionalPart);};
            apply(5);require(p.words==saved,"Optional presentation changed saved profile");validate();
            apply(6);require(preview.appearance().word==word,"Declining candidate did not restore original car");
            require(p.words==saved,"Restore changed saved profile");++offers;
        }
    }
    std::cout<<"Original tuning preview:35 cars,"<<offers<<" authored optional offers,"<<checks<<" geometry/texture/clock/restoration checks; no devices or user data writes.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
