#include "original_ranking_presentation.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
using namespace idas3::original;
int main(int argc,char**argv){try{
    if(argc!=2)throw std::runtime_error("project root required");const std::filesystem::path root=argv[1];
    auto records=OriginalRankingRecords::load(root/"data/original_assets/attract/ranking/factory_records.idasrank");
    OriginalRankingPresentation presentation;presentation.load(root);unsigned frames=0,draws=0,configurations=0;
    OriginalRankingPlayback p;
    for(unsigned course=0;course<9;++course){
        p.reset(course);const auto initial=originalRankingCourse(p.page);
        unsigned count=0;bool firstDraw=true;
        while(!p.completed()&&count<10810){
            p.step(records);++count;++frames;
            if(p.events.configureCar){++configurations;firstDraw=true;}
            if(p.events.drawCar){
                if(p.drawLayerMask!=(firstDraw?3u:2u))throw std::runtime_error("Source reflection mask lifetime");
                firstDraw=false;
                if(p.resources.slideTicks==29||(p.resources.phase==4&&count%300==0)){
                    const auto beforeTicks=p.ticks;const auto& mesh=presentation.mesh(p);
                    if(mesh.vertices.empty())throw std::runtime_error("Ranking mesh empty");
                    for(const auto& v:mesh.vertices)if(!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.position.z))throw std::runtime_error("Ranking mesh nonfinite");
                    const auto totalTextures=presentation.groundTextures().size()+presentation.carTextures().size()+presentation.plateTextures().size()+presentation.environmentTextures().size();
                    unsigned environmentDraws=0;
                    for(const auto& range:mesh.ranges){
                        if(range.texture!=0xffffffffu&&range.texture>=totalTextures)throw std::runtime_error("Ranking texture reference out of banks");
                        environmentDraws+=range.texture+1==totalTextures;
                        if(range.originalLightDirection)throw std::runtime_error("Ranking source does not change reflected light direction");
                    }
                    if(!environmentDraws)throw std::runtime_error("Ranking source environment layer is absent");
                    const auto& pixels=presentation.paint(1280,720,p,records);
                    if(pixels.size()!=1280*720||std::none_of(pixels.begin(),pixels.end(),[](auto v){return v>>24;}))throw std::runtime_error("Ranking board empty");
                    const auto* address=pixels.data();const auto copy=pixels;
                    if(presentation.paint(1280,720,p,records).data()!=address||copy!=pixels||p.ticks!=beforeTicks)throw std::runtime_error("Painting advanced playback or changed cached board");
                    ++draws;
                }
            }
        }
        const unsigned expected=initial==8?2102:3904;
        if(count!=expected||!p.completed())throw std::runtime_error("Original natural ranking duration "+std::to_string(course)+"="+std::to_string(count));
        const auto stopped=p.ticks;p.step(records);if(p.ticks!=stopped)throw std::runtime_error("Completed playback continued");
    }
    // Exercise every source car, beyond the factory board's subset. The
    // source selector3 image is a separate bank; repeated rendering cannot
    // rebuild materials or advance the resource/rotation clock.
    for(unsigned car=0;car<35;++car){
        OriginalRankingPlayback fixture;fixture.resources.hasCar=true;fixture.resources.car=car;
        fixture.resources.packedAppearance=0;fixture.events.drawCar=true;
        fixture.events.drawYawUnits=8192;fixture.drawLayerMask=2;
        const auto& mesh=presentation.mesh(fixture);const auto revision=presentation.textureRevision();
        const auto total=presentation.groundTextures().size()+presentation.carTextures().size()+presentation.plateTextures().size()+presentation.environmentTextures().size();
        const auto env=unsigned(total-1);unsigned environmentDraws=0;
        for(const auto& range:mesh.ranges){environmentDraws+=range.texture==env;
            if(range.texture!=0xffffffffu&&range.texture>=total)throw std::runtime_error("All-car ranking texture bound");}
        if(!environmentDraws)throw std::runtime_error("All-car source environment binding missing");
        const auto* address=mesh.vertices.data();
        if(presentation.mesh(fixture).vertices.data()!=address||presentation.textureRevision()!=revision||fixture.ticks)
            throw std::runtime_error("Environment rendering changed clock/resources");
    }
    std::cout<<"Ranking playback/presentation: "<<frames<<" source ticks, "<<configurations<<" car configurations, "<<draws<<" mesh/board checks; no renderer/device launched\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
