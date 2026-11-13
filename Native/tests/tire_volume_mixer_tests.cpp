#include "audio.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
using namespace idas3;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
std::vector<short> capture(const std::filesystem::path& root,float engine,float tires,bool inject,bool changeQueued=false,float master=1){
    EngineAudio audio;auto profile=original::makeOriginalFreshBattleProfile();profile.setu(16,0);
    audio.selectOriginalEngine(root,profile);audio.setOutputGains({master,0,engine,0,tires});
    std::uint32_t seed=1;original::OriginalEngineControlInput input;input.rpm=6500;input.throttle=1;input.gear=3;
    audio.stepOriginalEngine(input,seed);audio.finishSoundFrame(seed);
    std::vector<short> out;
    for(unsigned frame=0;frame<90;++frame){
        // Confirmed network audio and solo audio share this queued mixer.
        std::vector<original::OriginalTireCommand> commands;
        if(inject){if(frame==0)commands.push_back({original::OriginalTireCommandType::Play,0});commands.push_back({original::OriginalTireCommandType::Volume,127});}
        audio.applyConfirmedOnlineAudio({},commands);
        // A slider must affect already queued PCM immediately, without resynthesis.
        if(changeQueued)audio.setOutputGains({master,0,engine,0,0});
        for(unsigned i=0;i<735;++i){auto pcm=audio.renderStereo(6500,1,30,.7f,true);out.insert(out.end(),pcm.begin(),pcm.end());}
    }
    require(!audio.engineStatistics().droppedFrames&&!audio.engineStatistics().underflowFrames,"Audio queue lost frames");
    return out;
}
double energy(const std::vector<short>& pcm){double sum=0;for(auto x:pcm)sum+=double(x)*x;return sum;}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("project root required");
    const auto engine=capture(argv[1],1,0,true),withoutTires=capture(argv[1],1,0,false);
    require(engine==withoutTires,"Tire mute changed engine PCM");require(energy(engine)>1e8,"Engine-only output is silent");
    const auto tires=capture(argv[1],0,1,true),silence=capture(argv[1],0,0,true);
    require(energy(tires)>1e8,"Engine mute also muted tires");require(energy(silence)==0,"Both muted is not silent");
    require(capture(argv[1],1,1,true,true)==engine,"Tire slider did not affect queued audio");
    const auto half=capture(argv[1],0,.5f,true);require(std::abs(energy(half)/energy(tires)-.25)<.002,"Tire half-volume gain wrong");
    // The full-volume tire peaks at17692: doubling it exceeds signed16-bit
    // output. Verify slider linearity with master headroom, and separately
    // verify that the overloaded output is louder without hard clipping.
    const auto quietTires=capture(argv[1],0,1,true,false,.5f),quietEngine=capture(argv[1],1,0,true,false,.5f);
    const auto boostedTires=capture(argv[1],0,2,true,false,.5f),boostedEngine=capture(argv[1],2,0,true,false,.5f);
    require(std::abs(energy(boostedTires)/energy(quietTires)-4)<.01,"200% tire gain is not twice the amplitude with headroom");
    require(std::abs(energy(boostedEngine)/energy(quietEngine)-4)<.01,"200% engine gain is not twice the amplitude with headroom");
    const auto overloaded=capture(argv[1],0,2,true);int peak=0;
    for(auto sample:overloaded)peak=std::max(peak,std::abs(int(sample)));
    require(peak>=32110&&peak<=32112&&energy(overloaded)>3*energy(tires),"Overloaded tire boost clips or loses its volume increase");
    const auto together=capture(argv[1],1,1,true);
    for(std::size_t i=0;i<together.size();++i)require(std::abs(int(together[i])-int(engine[i])-int(tires[i]))<=2,"Separated channels do not reconstruct original mix");
    EngineAudio a;a.setOutputGains({.4f,.3f,.2f,.1f,.6f});
    for(float bad:{-1.f,2.1f,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        bool rejected=false;try{a.setOutputGains({1,1,1,1,bad});}catch(const std::invalid_argument&){rejected=true;}
        require(rejected&&a.outputGains().tires==.6f&&a.outputGains().engine==.2f,"Invalid gains changed active mix");
    }
    std::cout<<"PASS engine/tire isolation, mute, half and 200% amplitude with headroom, overload protection, queued-volume changes, reconstructed original mix, invalid gain rejection\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
