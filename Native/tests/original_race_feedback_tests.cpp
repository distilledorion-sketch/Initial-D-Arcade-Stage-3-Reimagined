#include "original_race_feedback.h"
#include "original_menu_audio.h"
#include "original_hud.h"
#include "sh4_scalar_reference.h"
#include <fstream>
#include <iostream>
#include <iterator>
using namespace idas3;
void require(bool b,const char* message){if(!b)throw std::runtime_error(message);}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::invalid_argument("project-root original-image required");
    using namespace idas3::reference;RefMemory m(argv[2]);
    constexpr unsigned obj=0xd000000,timer=obj+0x2000,hud=obj+0x2100,stack=0xd010000,stop=0x00ff0000;
    std::size_t instructions=0,refills=0,samples=0;
    for(int initial:{-100,0,5999,6000,530000})for(int bonus:{1,7,25,60})for(unsigned frame:{0u,1u,4u,5u,9u,20u,119u}){
        m.clear();m.zeroRegion(obj,0x3000);m.zeroRegion(stack,0x10000);
        m.write32(obj+1176,timer);m.write32(obj+1404,hud);m.write8(obj+1396,1);
        m.write32(obj+1392,unsigned(initial));m.write32(obj+1400,frame);
        const auto remaining=initial+bonus*6000-100;m.write32(timer+8,unsigned(remaining));
        RefCpu c(m);c.r[13]=obj;c.r[14]=stack;c.r[15]=stack+0xf000;
        c.callHooks[0xc2223b8]=[](auto& cpu){cpu.fpul=unsigned(signed32(cpu.r[4])/signed32(cpu.r[5]));};
        unsigned cues=0;c.callHooks[0xc1420c0]=[&](auto& cpu){require(cpu.r[4]==1&&cpu.r[5]==1,"wrong refill cue");++cues;};
        instructions+=c.run(0xc065642,0xc06570c,1000);
        OriginalRaceFeedback native;native.extend(initial);native.refillFrame=frame;
        const auto cue=native.tick(remaining);
        require(cues==unsigned(cue)&&m.read32(obj+1392)==unsigned(native.displayedRemaining)&&m.read32(hud+4)==unsigned(native.displayedRemaining),"refill value/cue differs from original");
        require(m.read8(obj+1396)==unsigned(native.refilling)&&m.read32(obj+1400)==native.refillFrame,"refill state differs");++refills;
    }
    // Actual countdown owner: no movement/render rate can advance this twice.
    for(unsigned priority=0;priority<8;++priority){
        m.clear();m.zeroRegion(obj,0x3000);m.zeroRegion(stack,0x10000);
        m.write32(timer+20,120);m.write32(timer+24,obj);OriginalRaceFeedback native;native.extend(100);
        for(unsigned frame=0;frame<120;++frame){
            m.write32(obj+1584,priority);RefCpu c(m);c.r[4]=timer;c.r[15]=stack+0xf000;c.pr=stop;
            instructions+=c.run(0xc05b0e0,stop,1000);native.tick(100);
            require(m.read32(timer+20)==native.extensionTicks,"extension lifetime differs");
            require(bool(m.read8(timer+16))==(native.extensionTicks==0),"extension removal boundary differs");
            require(m.read32(obj+1584)==(native.extensionTicks?std::max(priority,2u):priority),"extension priority differs");
        }
    }
    const auto originalHud=OriginalRaceHud::load(argv[1]);OriginalHudState state;state.flags=0;state.timeExtended=true;
    const auto commands=originalHud.drawList(state);require(commands.size()==1&&commands[0].index==4,"TIME EXTENDED original artwork missing");
    m.clear();m.zeroRegion(obj,0x3000);m.zeroRegion(stack,0x10000);
    m.write32(obj+104,0x400000);m.write32(stack+8,obj+64);RefCpu draw(m);draw.r[3]=obj+64;draw.r[13]=hud;draw.r[14]=stack;
    for(unsigned i=0;i<16;++i)draw.xf[i]=std::bit_cast<unsigned>(i%5==0?1.f:0.f);
    unsigned draws=0;draw.callHooks[0xc145ae0]=[&](auto& cpu){require(cpu.r[5]==commands[0].index,"original message chunk mismatch");for(unsigned i=0;i<16;++i)require(cpu.xf[i]==std::bit_cast<unsigned>(commands[0].matrix.elements[i]),"message matrix differs");++draws;};
    instructions+=draw.run(0xc0c9c32,0xc0c9cb2,1000);require(draws==1,"source message not drawn");
    for(unsigned bank:{2u,4u}){
        const auto path=std::filesystem::path(argv[1])/"data/original_audio/race"/(bank==2?"PACK22.dtpk":"PACK24.dtpk");
        std::ifstream in(path,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        auto u32=[&](unsigned p){return unsigned(bytes.at(p))|unsigned(bytes.at(p+1))<<8|unsigned(bytes.at(p+2))<<16|unsigned(bytes.at(p+3))<<24;};
        for(unsigned cue=0;cue<(bank==2?8u:6u);++cue){if(cue==5)continue;
            const auto sound=loadOriginalRaceSound(argv[1],bank,cue);
            const auto at=u32(u32(0x3c)+4+16*sound.sampleId);
            require(sound.clip.channels==1&&!sound.clip.looping,"unexpected race one-shot format");
            for(unsigned i=0;i<sound.clip.frames();++i){const auto pcm=std::bit_cast<std::uint16_t>(sound.clip.samples[i]);require((pcm&255)==bytes.at(at+2*i)&&(pcm>>8)==bytes.at(at+2*i+1),"original PCM mismatch");++samples;}
            m.clear();m.zeroRegion(obj,64);m.zeroRegion(stack,0x10000);m.write32(0xc8ff1cc+(bank==2?0:12),obj);
            m.write32(obj+40,bank==2?0xc31ec1c:0xc31ecc4);RefCpu c(m);c.r[4]=cue;c.r[5]=1;c.r[15]=stack+0xf000;c.pr=stop;
            unsigned queued=0;c.callHooks[0xc1ed9c0]=[&](auto& cpu){require(cpu.r[4]==sound.command,"race source command differs");++queued;};
            instructions+=c.run(bank==2?0xc141fc0:0xc1420c0,stop,1000);require(queued==1,"race cue not queued");
        }
        bool rejected=false;try{loadOriginalRaceSound(argv[1],bank,5);}catch(const std::exception&){rejected=true;}require(rejected,"compound sequence guessed as PCM one-shot");
    }
    std::cout<<"PASS "<<refills<<" original refill cases, 960 lifetime frames, original message chunk/matrix, 12 race cues / "<<samples<<" exact PCM samples, "<<instructions<<" original instructions. Hooks: bounded signed division, source sound queue, final HUD draw.\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
