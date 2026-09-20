#include "original_mode_menu.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <numeric>
#include <stdexcept>
using namespace idas3;
using namespace idas3::original;
using Clock=std::chrono::steady_clock;
struct ReferencePainter {
    NativeModel common,mode;NativeTextureBank commonTextures,modeTextures;
    explicit ReferencePainter(const std::filesystem::path& root){
        const auto p=root/"data/original_assets/menus/v3";
        common=NativeModel::load(p/"v3sS00common/v3sS00common.idasmesh");commonTextures=NativeTextureBank::load(p/"v3sS00common/textures/textures.idastex");
        mode=NativeModel::load(p/"v3sS11mode/v3sS11mode.idasmesh");modeTextures=NativeTextureBank::load(p/"v3sS11mode/textures/textures.idastex");
    }
    std::vector<std::uint32_t> paint(const OriginalModeMenuState& state){
        std::vector<std::uint32_t> out(640*480,0xff000000u);
        // The pre-optimization materialization path and existing independent
        // general compositor remain the pixel reference, without prepared data.
        for(const auto& d:originalModeMenuDraws(state)){
            const auto& model=d.bank==8?common:mode;const auto& textures=d.bank==8?commonTextures:modeTextures;
            SpritePlacement p;p.scale=100.f*d.scale;p.offsetX=d.x*100.f;p.offsetY=-d.y*100.f;p.invertY=true;p.authoredHeight=0;
            if(d.replaceVertexColors||d.replaceMaterialDiffuse){
                auto chunk=model.chunks.at(d.chunk);
                for(auto& batch:chunk.batches){
                    if(d.replaceVertexColors){batch.material[2]=0x600u;if(batch.ich[6]==0x4au)for(auto& v:batch.vertices){v.color0=d.color;v.color1=0;}}
                    if(d.replaceMaterialDiffuse){batch.material[3]=d.color;batch.material[5]=d.color;}
                }
                compositeOriginalMenuChunk(out,640,480,textures,chunk,p);
            }else compositeOriginalMenuChunk(out,640,480,textures,model.chunks.at(d.chunk),p);
        }
        return out;
    }
};
int main(int argc,char** argv){try{
    if(argc<2)throw std::runtime_error("usage: original_mode_raster_tests game_root");
    OriginalModeMenu menu;menu.load(argv[1]);ReferencePainter reference(argv[1]);
    std::size_t frames=0;std::vector<double> timings;double first=0;
    auto verify=[&](const OriginalModeMenuState& state,bool timed){
        const auto t=Clock::now();const auto& actual=menu.paint(640,480,state);
        const double ms=std::chrono::duration<double,std::milli>(Clock::now()-t).count();if(!frames)first=ms;if(timed)timings.push_back(ms);
        const auto expected=reference.paint(state);
        if(actual!=expected){auto i=std::mismatch(actual.begin(),actual.end(),expected.begin()).first-actual.begin();
            throw std::runtime_error("Mode raster mismatch frame="+std::to_string(frames)+" selected="+std::to_string(unsigned(state.selected))+" phase="+std::to_string(state.confirmationPhase)+" pixel="+std::to_string(i)+" actual="+std::to_string(actual[i])+" expected="+std::to_string(expected[i]));}
        ++frames;
    };
    for(unsigned selected=0;selected<3;++selected){OriginalModeMenuState state;state.selected=OriginalGameMode(selected);state.selectedFrames=60;
        verify(state,false);for(unsigned tick=1;tick<=120;++tick){state.confirmationPhase=float(tick)/120.f;++state.selectedFrames;verify(state,true);}}
    for(unsigned old=0;old<3;++old)for(unsigned next=0;next<3;++next){OriginalModeMenuState state;state.selected=OriginalGameMode(old);state.selectedFrames=21;
        selectOriginalGameMode(state,OriginalGameMode(next));for(unsigned frame=0;frame<21;++frame){verify(state,false);stepOriginalModeMenu(state);}}
    for(unsigned selected=0;selected<3;++selected)for(float phase:{.009999999776482582f,.01000001f,.12345f,.49999997f,.50000006f,.99999f}){
        OriginalModeMenuState state;state.selected=OriginalGameMode(selected);state.selectedFrames=7;state.confirmationPhase=phase;verify(state,false);}
    OriginalModeMenuState last;last.selected=OriginalGameMode::BuntaChallenge;last.selectedFrames=50;last.confirmationPhase=.321f;const auto original=menu.paint(640,480,last);const auto& enlarged=menu.paint(1280,960,last);
    for(unsigned y=0;y<960;++y)for(unsigned x=0;x<1280;++x)if(enlarged[y*1280+x]!=original[(y/2)*640+x/2])throw std::runtime_error("Window scaling changed a prepared original pixel");
    const auto cacheBytes=menu.preparedRasterBytes();if(!cacheBytes||cacheBytes>32u*1024u*1024u)throw std::runtime_error("Mode raster cache did not respect its memory bound");
    menu.load(argv[1]);if(menu.preparedRasterBytes()!=0)throw std::runtime_error("Reload retained stale prepared model data");verify(last,false);
    std::sort(timings.begin(),timings.end());
    std::cout<<"PASS "<<frames<<" frames / "<<frames*640ull*480<<" pixels exactly match the existing general compositor; all360 confirmation updates, focus transitions, phase boundaries and2x scaling.\n";
    std::cout<<"confirmation_ms mean="<<std::accumulate(timings.begin(),timings.end(),0.)/timings.size()<<" p95="<<timings[timings.size()*95/100]<<" max="<<timings.back()<<" first_frame="<<first<<'\n';
    std::cout<<"prepared_cache_bytes="<<cacheBytes<<" hard_limit_bytes="<<32u*1024u*1024u<<"; reload invalidation passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
