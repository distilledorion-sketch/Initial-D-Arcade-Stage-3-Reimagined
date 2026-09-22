#include "original_engine_playback.h"
#include "original_tuning.h"
#include <algorithm>
#include <fstream>
#include <iostream>
using namespace idas3;
using namespace idas3::original;
static unsigned checks=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
void wav(const std::filesystem::path& file,const std::vector<short>& pcm){
    std::ofstream f(file,std::ios::binary);auto w=[&](unsigned v,unsigned n){for(unsigned i=0;i<n;++i)f.put(char(v>>(i*8)));};
    f.write("RIFF",4);w(36+unsigned(pcm.size()*2),4);f.write("WAVEfmt ",8);w(16,4);w(1,2);w(2,2);w(44100,4);w(176400,4);w(4,2);w(16,2);
    f.write("data",4);w(unsigned(pcm.size()*2),4);for(auto v:pcm)w(unsigned(std::uint16_t(v)),2);
}
int main(int argc,char** argv)try{
    if(argc<2||argc>3)throw std::runtime_error("native-root [capture-directory]");
    const auto data=OriginalTuningData::load(argv[1]);unsigned profiles=0;
    // Exercise every original car/package/performance level, including optional engine flags.
    for(unsigned car=0;car<35;++car)for(unsigned package=0;package<data.car(car).packages.size();++package)
    for(unsigned level=0;level<76;++level)for(unsigned flags=0;flags<4;++flags){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,car);p.setByte(152,std::uint8_t(package));p.setByte(164,std::uint8_t(level));p.setByte(162,flags&1);p.setByte(166,(flags>>1)&1);
        const auto selected=selectOriginalEngineSound(p);const auto raw=configureOriginalEngine(selected.family,selected.level,p.byte(152),p.byte(162),p.byte(166));
        const auto actual=configureProfileEngineSound(p);auto expected=raw;
        if(car==1&&package==0&&level<3)expected.auxiliaryLoop=expected.releaseCue=false;
        require(actual.family==expected.family&&actual.originalCar==expected.originalCar&&actual.pitch0==expected.pitch0&&actual.volume0==expected.volume0&&actual.volumePitch1==expected.volumePitch1&&actual.backfire==expected.backfire&&actual.auxiliaryLoop==expected.auxiliaryLoop&&actual.releaseCue==expected.releaseCue,"Unrelated engine configuration changed");++profiles;
    }
    unsigned stages=0;
    // Apply real authored basic tuning commands, including intervening cosmetic upgrades.
    for(unsigned package=0;package<5;++package){
        auto p=makeOriginalFreshBattleProfile();p.setu(16,1);p.setByte(152,std::uint8_t(package));p.setu(1180,p.u(1180)|1);p.setu(72,999999);
        OriginalEnginePlayback sound(argv[1]),muted(argv[1]);
        while(!(p.u(1180)&0x400)){
            const auto index=p.byte(153);applyOriginalTuningCommand(p,data,1);
            require(p.byte(153)!=index||(p.u(1180)&0x400),"Upgrade did not progress");
            const unsigned step=p.byte(164);const bool turbo=package==0?(step==3||step==4):(package==3&&step>=5);
            const bool boost=turbo||(package==0&&step>=5);
            sound.select(p);muted.select(p);
            require(sound.configuration().auxiliaryLoop==boost,"Boost loop enabled before installed upgrade");
            require(sound.configuration().releaseCue==turbo,"Turbo release does not match installed upgrade");
            std::uint32_t seed=7,quietSeed=7;unsigned releases=0;double auxEnergy=0,totalEnergy=0;std::vector<short> pcm;
            const unsigned frames=package==0&&step==5?720:240;
            for(unsigned frame=0;frame<frames;++frame){
                OriginalEngineControlInput input;input.rpm=3000.f+float(frame%120)*35.f;input.throttle=frame%120<100?1.f:0.f;input.gear=frame%120<70?2:3;
                sound.step(input,seed,[&](unsigned bank,unsigned){if(bank==5)++releases;});muted.step(input,quietSeed,{});
                const OriginalEngineCommand mute{OriginalEngineCommandTarget::Continuous,3,0x10a5,0};muted.applyContinuous(std::span(&mute,1));
                sound.finishSoundFrame();muted.finishSoundFrame();
                double frameEnergy=0;
                for(unsigned i=0;i<735;++i){const auto a=sound.renderFrame(),b=muted.renderFrame();for(unsigned channel=0;channel<2;++channel){
                    const double difference=double(a.dry[channel])-b.dry[channel];auxEnergy+=difference*difference;frameEnergy+=double(a.dry[channel])*a.dry[channel];
                    if(argc==3&&package==0&&(step==2||step==3||step==5))pcm.push_back(short(std::clamp(a.dry[channel],-32768,32767)));
                }}
                totalEnergy+=frameEnergy;if(package==0&&step==5&&frame>3)require(frameEnergy>0,"Step 5 audio dropped out");
            }
            require((releases>0)==turbo,"Actual throttle/shift release cues incorrect");
            require(boost?auxEnergy>1e6:auxEnergy==0,"Boost loop PCM missing or unexpectedly audible");
            require(totalEnergy>1e9,"Engine PCM silent");
            if(argc==3&&package==0&&(step==2||step==3||step==5)){std::filesystem::create_directories(argv[2]);wav(std::filesystem::path(argv[2])/("levin-a-step-"+std::to_string(step)+".wav"),pcm);}
            ++stages;
        }
    }
    std::cout<<"PASS Levin tuning audio: "<<profiles<<" car/package/level/option configurations; "<<stages<<" real upgrade transitions; "<<checks<<" checks. A: no boost before Step 3, turbo at 3/4, continuous non-release audio at 5; B/C/basic remain NA, D turbo at 5. Step 5 sustained PCM checked for 12 seconds.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
